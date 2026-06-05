/**
 * F-PSU Network Benchmark — Asiacrypt 2026
 * Client/server binary that runs the F-PSU protocol over TCP.
 *
 * Epoch 0: full IBLT exchange (one-time O(n) cost).
 * Epoch >0: sparse delta IBLT exchange (O(k|Δ|) communication).
 *
 * Build:  cd build && cmake .. && make -j
 * Usage:
 *   Server:  ./fpsu_net -s -p PORT
 *   Client:  ./fpsu_net -c HOST -p PORT
 */

#include "iblt.h"
#include "protocol.h"
#include "socket.h"

#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <unordered_set>
#include <vector>

using Clock = std::chrono::high_resolution_clock;
using Ms = std::chrono::duration<double, std::milli>;

// ============================================================
// Test data generation (deterministic)
// ============================================================
#include <cryptoTools/Common/block.h>
#include <cryptoTools/Crypto/PRNG.h>
using namespace osuCrypto;

static std::vector<std::string> gen_items(size_t n, size_t seed) {
    std::vector<std::string> items;
    items.reserve(n);
    PRNG prng(block(seed, 0));
    for (size_t i = 0; i < n; ++i) {
        uint64_t val = prng.get<uint64_t>();
        items.emplace_back(reinterpret_cast<const char*>(&val), sizeof(val));
    }
    return items;
}

// ============================================================
// Command-line parsing
// ============================================================
struct Config {
    bool   server     = false;
    std::string host;
    int    port       = 8888;
    size_t baseN      = 1024;
    size_t deltaN     = 256;
    double lambda     = 2.0;
    int    k          = 4;
    int    epochs     = 5;
    double rttMs      = 0.0;  // simulated RTT (ms)
    double bwMbps     = 0.0;  // bandwidth limit (Mbps, 0 = unlimited)
    bool   staticMode = false; // -S: full IBLT every epoch (static baseline)
};

static void usage(const char* prog) {
    std::cerr << "Usage:\n"
              << "  Server: " << prog << " -s -p PORT [opts]\n"
              << "  Client: " << prog << " -c HOST -p PORT [opts]\n"
              << "Options:\n"
              << "  -n N       Set size per party (default 1024)\n"
              << "  -d N       Delta size per epoch (default 256)\n"
              << "  -l F       IBLT lambda factor (default 2.0)\n"
              << "  -k N       IBLT hash count (default 4)\n"
              << "  -e N       Number of epochs (default 5)\n"
              << "  -r N       Simulated RTT in ms (default 0)\n"
              << "  -b N       Simulated bandwidth in Mbps (default 0=unlimited)\n"
              << "  -S         Static baseline: full IBLT every epoch (no delta)\n";
    exit(1);
}

static Config parse_args(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-s") cfg.server = true;
        else if (a == "-c" && i + 1 < argc) cfg.host = argv[++i];
        else if (a == "-p" && i + 1 < argc) cfg.port = std::stoi(argv[++i]);
        else if (a == "-n" && i + 1 < argc) cfg.baseN = std::stoull(argv[++i]);
        else if (a == "-d" && i + 1 < argc) cfg.deltaN = std::stoull(argv[++i]);
        else if (a == "-l" && i + 1 < argc) cfg.lambda = std::stod(argv[++i]);
        else if (a == "-k" && i + 1 < argc) cfg.k = std::stoi(argv[++i]);
        else if (a == "-e" && i + 1 < argc) cfg.epochs = std::stoi(argv[++i]);
        else if (a == "-r" && i + 1 < argc) cfg.rttMs = std::stod(argv[++i]);
        else if (a == "-b" && i + 1 < argc) cfg.bwMbps = std::stod(argv[++i]);
        else if (a == "-S") cfg.staticMode = true;
        else usage(argv[0]);
    }
    bool is_client = !cfg.host.empty();
    if (cfg.server == is_client) usage(argv[0]);
    return cfg;
}

// ============================================================
// Network simulation (WAN latency + bandwidth)
// ============================================================
#include <thread>

struct NetSim {
    double rttMs;
    double bwMbps;

    // Simulate one-way network delay for sending `n` bytes.
    // Called only before send operations (not recv).
    void delaySend(size_t bytes) const {
        double s = 0;
        if (rttMs > 0)  s += rttMs * 0.0005;  // half RTT (one-way)
        if (bwMbps > 0) s += (bytes * 8.0) / (bwMbps * 1e6);
        if (s > 0)
            std::this_thread::sleep_for(
                std::chrono::microseconds((int64_t)(s * 1e6)));
    }
};

// ============================================================
// IBLT exchange helpers (with network simulation)
// ============================================================
static bool sendIbltFull(fpsu::net::TcpSocket& sock, const fpsu::IBLT& iblt,
                          const NetSim& sim) {
    auto buf = iblt.serialize();
    sim.delaySend(buf.size());
    return sock.send(buf.data(), buf.size());
}

static bool recvIbltFull(fpsu::net::TcpSocket& sock, fpsu::IBLT& iblt) {
    auto buf = sock.recv();
    if (buf.empty()) return false;
    iblt.deserialize(buf);
    return true;
}

static bool sendIbltDelta(fpsu::net::TcpSocket& sock,
                           const fpsu::IBLT& deltaIblt,
                           const NetSim& sim) {
    auto buf = deltaIblt.deltaSerialize();
    sim.delaySend(buf.size());
    return sock.send(buf.data(), buf.size());
}

static bool recvIbltDelta(fpsu::net::TcpSocket& sock,
                           fpsu::IBLT& peerIbltCopy) {
    auto buf = sock.recv();
    if (buf.empty()) return false;
    peerIbltCopy.deltaDeserialize(buf);
    return true;
}

// ============================================================
// Epoch timing record
// ============================================================
struct EpochRecord {
    int    epoch;
    double encodeMs;
    double xferMs;
    double peelMs;
    double totalMs;
    size_t unionSize;
    int    cascade;
    size_t xferBytes;
};

// ============================================================
// PartyState — tracks IBLT, raw set, delta IBLT, and peer's copy
// ============================================================
struct PartyState {
    fpsu::FpsuParty party;
    std::unordered_set<std::string> rawSet;
    fpsu::IBLT deltaIblt;       // accumulated delta since last exchange
    fpsu::IBLT peerIbltCopy;    // copy of peer's IBLT (maintained via deltas)

    PartyState(const std::string& key, int k, int ell,
               const std::vector<std::string>& init)
        : party(key, k, ell), deltaIblt(k, ell), peerIbltCopy(k, ell) {
        party.initialize(init);
        for (auto& x : init) rawSet.insert(x);
    }

    void applyDeltas(const std::vector<std::string>& adds,
                     const std::vector<std::string>& rems,
                     const std::unordered_set<std::string>* peerSet = nullptr) {
        for (auto& x : adds) {
            if (rawSet.insert(x).second) {
                party.iblt().insert(x);
                deltaIblt.insert(x);
            }
        }
        for (auto& x : rems) {
            if (rawSet.erase(x)) {
                party.iblt().remove(x);
                if (!peerSet || peerSet->count(x))
                    deltaIblt.remove(x);
            }
        }
    }

    void clearDelta() { deltaIblt = fpsu::IBLT(party.iblt().k(), party.iblt().ell()); }
};

// Generate remove candidates from items already in the set.
static std::vector<std::string> gen_rems_from_set(
    std::unordered_set<std::string>& s, size_t n, size_t seed) {
    std::vector<std::string> out;
    out.reserve(n);
    PRNG prng(block(seed, 0));
    while (out.size() < n && !s.empty()) {
        size_t skip = prng.get<uint64_t>() % s.size();
        auto it = s.begin();
        std::advance(it, skip);
        out.push_back(*it);
        s.erase(it);
    }
    return out;
}

// ============================================================
// Run one epoch (common to server and client)
// ============================================================
static EpochRecord run_epoch(PartyState& state,
                              fpsu::net::TcpSocket& sock,
                              const std::vector<std::string>& adds,
                              const std::vector<std::string>& rems,
                              int ep, int k, int ell, bool sendFirst,
                              const NetSim& sim, bool staticMode) {
    auto t0 = Clock::now();
    size_t xferBytes = 0;

    // Round 1: local update
    state.applyDeltas(adds, rems);
    auto t1 = Clock::now();

    // Collect modified cells from our own delta (before clearing)
    std::set<std::pair<int,int>> ourDeltaCells;
    std::vector<uint8_t> peerDeltaBuf; // raw bytes for start-set parsing

    // Round 2: exchange IBLTs
    if (ep == 0 || staticMode) {
        // Full IBLT exchange (epoch 0 bootstrap, or static baseline every epoch)
        if (sendFirst) {
            xferBytes += state.party.iblt().serializedBytes();
            if (!sendIbltFull(sock, state.party.iblt(), sim)) return {};
            if (!recvIbltFull(sock, state.peerIbltCopy)) return {};
        } else {
            if (!recvIbltFull(sock, state.peerIbltCopy)) return {};
            xferBytes += state.party.iblt().serializedBytes();
            if (!sendIbltFull(sock, state.party.iblt(), sim)) return {};
        }
        state.clearDelta();
    } else {
        // Subsequent epochs: exchange DELTA IBLTs (O(k|Δ|) communication)
        ourDeltaCells = state.deltaIblt.modifiedCells();
        auto deltaBuf = state.deltaIblt.deltaSerialize();
        xferBytes = deltaBuf.size();
        if (sendFirst) {
            sim.delaySend(deltaBuf.size());
            if (!sock.send(deltaBuf.data(), deltaBuf.size())) return {};
            peerDeltaBuf = sock.recv();
            if (peerDeltaBuf.empty()) return {};
            state.peerIbltCopy.deltaDeserialize(peerDeltaBuf);
        } else {
            peerDeltaBuf = sock.recv();
            if (peerDeltaBuf.empty()) return {};
            state.peerIbltCopy.deltaDeserialize(peerDeltaBuf);
            sim.delaySend(deltaBuf.size());
            if (!sock.send(deltaBuf.data(), deltaBuf.size())) return {};
        }
        state.clearDelta();
    }
    auto t2 = Clock::now();

    // Merge + peel on a COPY (preserve party's IBLT for next epoch)
    fpsu::IBLT merged = state.party.iblt();
    merged.mergeAdd(state.peerIbltCopy);

    // Build start set for peeling.
    // Epoch >0: prioritize delta-modified cells in the queue, then
    // fall back to all non-zero cells to guarantee completeness.
    // The cascade size metric captures how far the delta cascade reaches.
    std::set<std::pair<int, int>> start;
    if (ep == 0) {
        for (int i = 0; i < k; ++i)
            for (int j = 0; j < ell; ++j)
                if (merged.cell(i, j).count != 0)
                    start.insert({i, j});
    } else {
        // Delta cells first (cascade seeds)
        start.insert(ourDeltaCells.begin(), ourDeltaCells.end());
        auto peerCells = fpsu::IBLT::deltaCells(peerDeltaBuf);
        start.insert(peerCells.begin(), peerCells.end());
        // Then all remaining non-zero cells (guarantee completeness)
        for (int i = 0; i < k; ++i)
            for (int j = 0; j < ell; ++j)
                if (merged.cell(i, j).count != 0)
                    start.insert({i, j});
    }

    auto [peeled, cascade, ok] = merged.incrementalPeel(start);
    auto t3 = Clock::now();

    EpochRecord rec;
    rec.epoch     = ep;
    rec.encodeMs  = std::chrono::duration_cast<Ms>(t1 - t0).count();
    rec.xferMs    = std::chrono::duration_cast<Ms>(t2 - t1).count();
    rec.peelMs    = std::chrono::duration_cast<Ms>(t3 - t2).count();
    rec.totalMs   = std::chrono::duration_cast<Ms>(t3 - t0).count();
    rec.unionSize = peeled.size();
    rec.cascade   = cascade;
    rec.xferBytes = xferBytes;

    const char* xferType = (ep == 0 || staticMode) ? "FULL" : "DELTA";
    std::cout << "  Epoch " << ep << " (" << xferType << "): |U|=" << peeled.size()
              << " cascade=" << cascade
              << " enc=" << std::fixed << std::setprecision(2) << rec.encodeMs << "ms"
              << " xfer=" << rec.xferMs << "ms"
              << " peel=" << rec.peelMs << "ms"
              << " total=" << rec.totalMs << "ms"
              << " bytes=" << rec.xferBytes
              << (ok ? "" : " WARN:incomplete") << std::endl;
    return rec;
}

// ============================================================
// Server side
// ============================================================
static int run_server(const Config& cfg) {
    fpsu::net::TcpSocket listener;
    if (!listener.listen(cfg.port)) return 1;

    auto sock = listener.accept();
    if (!sock.valid()) return 1;
    listener.close();

    int ell = static_cast<int>(cfg.lambda * (cfg.baseN + cfg.deltaN) * 2 / cfg.k) + 1;
    PartyState state("server_key", cfg.k, ell, gen_items(cfg.baseN, 42));

    std::vector<EpochRecord> records;
    records.reserve(cfg.epochs);
    NetSim sim{cfg.rttMs, cfg.bwMbps};

    for (int ep = 0; ep < cfg.epochs; ++ep) {
        auto adds = gen_items(cfg.deltaN, 1000 + ep * 10 + 0);
        auto rems = gen_rems_from_set(state.rawSet, cfg.deltaN / 4, 5000 + ep * 10);
        auto rec = run_epoch(state, sock, adds, rems, ep, cfg.k, ell, true, sim, cfg.staticMode);
        if (rec.totalMs < 0) return 1;
        records.push_back(rec);
    }

    double sumTotal = 0, sumXfer = 0;
    for (auto& r : records) { sumTotal += r.totalMs; sumXfer += r.xferMs; }
    std::cout << "\n=== Server Summary ===\n"
              << "  Epochs: " << cfg.epochs
              << "  Avg total: " << std::fixed << std::setprecision(2)
              << (sumTotal / cfg.epochs) << "ms"
              << "  Avg xfer: " << (sumXfer / cfg.epochs) << "ms"
              << "  IBLT full: " << (ell * cfg.k * 12) << " bytes\n";
    return 0;
}

// ============================================================
// Client side
// ============================================================
static int run_client(const Config& cfg) {
    fpsu::net::TcpSocket sock;
    if (!sock.connect(cfg.host.c_str(), cfg.port)) return 1;

    int ell = static_cast<int>(cfg.lambda * (cfg.baseN + cfg.deltaN) * 2 / cfg.k) + 1;
    PartyState state("client_key", cfg.k, ell, gen_items(cfg.baseN, 99));

    std::vector<EpochRecord> records;
    records.reserve(cfg.epochs);
    NetSim sim{cfg.rttMs, cfg.bwMbps};

    for (int ep = 0; ep < cfg.epochs; ++ep) {
        auto adds = gen_items(cfg.deltaN, 2000 + ep * 10 + 0);
        auto rems = gen_rems_from_set(state.rawSet, cfg.deltaN / 4, 6000 + ep * 10);
        auto rec = run_epoch(state, sock, adds, rems, ep, cfg.k, ell, false, sim, cfg.staticMode);
        if (rec.totalMs < 0) return 1;
        records.push_back(rec);
    }

    double sumTotal = 0, sumXfer = 0;
    for (auto& r : records) { sumTotal += r.totalMs; sumXfer += r.xferMs; }
    std::cout << "\n=== Client Summary ===\n"
              << "  Epochs: " << cfg.epochs
              << "  Avg total: " << std::fixed << std::setprecision(2)
              << (sumTotal / cfg.epochs) << "ms"
              << "  Avg xfer: " << (sumXfer / cfg.epochs) << "ms\n";
    return 0;
}

// ============================================================
// Main
// ============================================================
int main(int argc, char** argv) {
    Config cfg = parse_args(argc, argv);

    std::cout << std::string(64, '=') << std::endl;
    std::cout << "  F-PSU Network Benchmark" << std::endl;
    std::cout << "  Role: " << (cfg.server ? "Server" : "Client")
              << "  n=" << cfg.baseN << " |Δ|=" << cfg.deltaN
              << "  λ=" << cfg.lambda << "  k=" << cfg.k
              << "  epochs=" << cfg.epochs;
    if (cfg.staticMode) std::cout << "  [STATIC baseline]";
    std::cout << std::endl;
    if (cfg.rttMs > 0 || cfg.bwMbps > 0) {
        std::cout << "  Sim: RTT=" << cfg.rttMs << "ms"
                  << " BW=" << cfg.bwMbps << "Mbps" << std::endl;
    }
    std::cout << std::string(64, '=') << std::endl;

    if (cfg.server)
        return run_server(cfg);
    else
        return run_client(cfg);
}
