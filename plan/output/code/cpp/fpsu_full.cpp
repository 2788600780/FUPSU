/**
 * F-PSU Full Protocol Benchmark with OT/OPRF — Asiacrypt 2026
 *
 * Runs both parties in a single process with real Base OT + KOS OT Extension.
 * OT operations use the eval() pattern from benchmark.cpp (two coroutines
 * running concurrently). IBLT data exchange is synchronous in-process.
 *
 * Build:  cd build && cmake .. && make -j
 * Usage:  ./fpsu_full [n [delta [lambda [epochs [trials]]]]]
 */

#include "iblt.h"
#include "protocol.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

#include <coproto/Socket/LocalAsyncSock.h>
namespace cp = coproto;

#include <macoro/sync_wait.h>
#include <macoro/when_all.h>
#include <macoro/task.h>

#include <libOTe/Base/SimplestOT.h>
#include <libOTe/TwoChooseOne/Kos/KosOtExtReceiver.h>
#include <libOTe/TwoChooseOne/Kos/KosOtExtSender.h>

#include <cryptoTools/Common/block.h>
#include <cryptoTools/Common/BitVector.h>
#include <cryptoTools/Crypto/PRNG.h>

using namespace osuCrypto;
using Clock = std::chrono::high_resolution_clock;
using Ms = std::chrono::duration<double, std::milli>;

// ============================================================
// eval helper (same as benchmark.cpp)
// ============================================================
template<typename T0, typename T1>
inline void eval(macoro::task<T0> t0, macoro::task<T1> t1) {
    auto r = macoro::sync_wait(macoro::when_all_ready(std::move(t0), std::move(t1)));
    std::string e0, e1; bool threw = false;
    try { std::get<0>(r).result(); }
    catch (const std::exception& e) { e0 = e.what(); threw = true; }
    try { std::get<1>(r).result(); }
    catch (const std::exception& e) { e1 = e.what(); threw = true; }
    if (threw) throw std::runtime_error("P0: " + e0 + "\nP1: " + e1);
}

// ============================================================
// Test data generation
// ============================================================
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
// Configuration
// ============================================================
struct FullConfig {
    size_t baseN   = 1024;
    size_t deltaN  = 256;
    double lambda  = 3.0;
    int    k       = 4;
    int    epochs  = 5;
    int    trials  = 3;
};

// ============================================================
// Per-party state
// ============================================================
struct PartyRuntime {
    std::unordered_set<std::string> rawSet;
    fpsu::IBLT iblt;
    fpsu::IBLT deltaIblt;
    fpsu::IBLT peerCopy;
    std::string prfKey;

    PartyRuntime(int k, int ell)
        : iblt(k, ell), deltaIblt(k, ell), peerCopy(k, ell) {}

    void init(const std::vector<std::string>& items) {
        iblt.encode(items);
        for (const auto& x : items) rawSet.insert(x);
    }

    void applyDeltas(const std::vector<std::string>& adds,
                     const std::vector<std::string>& rems,
                     const std::unordered_set<std::string>* peerSet = nullptr) {
        for (const auto& x : adds) {
            if (rawSet.insert(x).second) {
                iblt.insert(x); deltaIblt.insert(x);
            }
        }
        for (const auto& x : rems) {
            if (rawSet.erase(x)) {
                iblt.remove(x);
                // Unique deletions: local-only, skip delta IBLT
                if (!peerSet || peerSet->count(x))
                    deltaIblt.remove(x);
            }
        }
    }

    void clearDelta() { deltaIblt = fpsu::IBLT(iblt.k(), iblt.ell()); }
};

// ============================================================
// Timing record
// ============================================================
struct EpochTiming {
    double otExtMs, encodeMs, xferMs, peelMs, keyMs, totalMs;
    size_t xferBytes, unionSize;
    int cascade;
};

// ============================================================
// Run OT extension (same pattern as benchmark.cpp)
// ============================================================
static double runKosOtExt(
    size_t nOTs,
    const std::vector<std::array<block, 2>>& baseSend,
    const BitVector& baseChoice,
    const std::vector<block>& baseRecv,
    size_t seed)
{
    auto sock = cp::LocalAsyncSocket::makePair();
    PRNG prng0(block(seed, 0));
    PRNG prng1(block(seed, 1));

    KosOtExtSender kosSender;
    KosOtExtReceiver kosReceiver;

    // Set base OTs (mutable copies for span)
    auto baseRecvCopy = baseRecv;
    auto baseChoiceCopy = baseChoice;
    auto baseSendCopy = baseSend;
    kosSender.setBaseOts(baseRecvCopy, baseChoiceCopy);
    kosReceiver.setBaseOts(baseSendCopy);

    std::vector<std::array<block, 2>> msgs(nOTs);
    for (auto& m : msgs)
        m = {block(prng0.get<uint64_t>(), prng0.get<uint64_t>()),
             block(prng0.get<uint64_t>(), prng0.get<uint64_t>())};
    BitVector choices(nOTs);
    choices.randomize(prng1);
    std::vector<block> out(nOTs);

    auto t0 = Clock::now();
    auto p0 = kosSender.send(msgs, prng0, sock[0]);
    auto p1 = kosReceiver.receive(choices, out, prng1, sock[1]);
    eval(std::move(p0), std::move(p1));
    return std::chrono::duration_cast<Ms>(Clock::now() - t0).count();
}

// ============================================================
// Run one full benchmark (synchronous per-epoch, OT via eval)
// ============================================================
struct BenchResult {
    size_t n, delta;
    double avgEpoch0Ms, avgEpochGt0Ms, avgOtExtMs, avgPeelMs;
    size_t avgXferBytes;
    double reduction;
};

static BenchResult run_one(const FullConfig& cfg) {
    int ell = static_cast<int>(cfg.lambda * (cfg.baseN + cfg.deltaN) * 2 / cfg.k) + 1;

    std::vector<double> allEpoch0, allEpochGt0, allOtExt, allPeel;
    std::vector<size_t> allXfer;

    for (int trial = 0; trial < cfg.trials; ++trial) {
        PartyRuntime partyA(cfg.k, ell);
        PartyRuntime partyB(cfg.k, ell);
        partyA.init(gen_items(cfg.baseN, 42 + trial * 10000));
        partyB.init(gen_items(cfg.baseN, 99 + trial * 10000));

        // ── Base OT (once per trial) ──
        std::vector<std::array<block, 2>> baseSend(128);
        BitVector baseChoice(128);
        std::vector<block> baseRecv(128);
        {
            PRNG prngOt(block(42 + trial * 10000, 9999));
            baseChoice.randomize(prngOt);
            auto basePair = cp::LocalAsyncSocket::makePair();
            SimplestOT baseSender, baseReceiver;
            auto t0 = baseReceiver.receive(baseChoice, baseRecv, prngOt, basePair[0]);
            auto t1 = baseSender.send(baseSend, prngOt, basePair[1]);
            eval(std::move(t0), std::move(t1));
        }

        // ── Per-epoch protocol ──
        for (int ep = 0; ep < cfg.epochs; ++ep) {
            auto t0 = Clock::now();

            // 1. Local delta update
            auto t1 = Clock::now();
            auto addsA = gen_items(cfg.deltaN, 42 + trial * 10000 + (ep + 1) * 1000);
            auto remsA = gen_rems_from_set(partyA.rawSet, cfg.deltaN / 4,
                                            42 + trial * 10000 + (ep + 1) * 5000);
            auto addsB = gen_items(cfg.deltaN, 99 + trial * 10000 + (ep + 1) * 1000);
            auto remsB = gen_rems_from_set(partyB.rawSet, cfg.deltaN / 4,
                                            99 + trial * 10000 + (ep + 1) * 5000);
            partyA.applyDeltas(addsA, remsA, &partyB.rawSet);
            partyB.applyDeltas(addsB, remsB, &partyA.rawSet);
            auto encodeMs = std::chrono::duration_cast<Ms>(Clock::now() - t1).count();

            // 2. OT Extension
            auto t2 = Clock::now();
            size_t cascadeEst = 5 * cfg.k * cfg.deltaN;
            double otExtMs = runKosOtExt(cascadeEst, baseSend, baseChoice, baseRecv,
                                          42 + trial * 10000 + ep * 5000);
            auto t3 = Clock::now();

            // 3. IBLT data exchange (synchronous in-process)
            size_t xferBytes = 0;
            fpsu::IBLT mergedA = partyA.iblt;
            if (ep == 0) {
                // Full IBLT exchange
                auto bufA = partyA.iblt.serialize();
                auto bufB = partyB.iblt.serialize();
                partyA.peerCopy.deserialize(bufB);
                partyB.peerCopy.deserialize(bufA);
                xferBytes = bufA.size();
                partyA.clearDelta(); partyB.clearDelta();
            } else {
                // Delta IBLT exchange
                auto dbA = partyA.deltaIblt.deltaSerialize();
                auto dbB = partyB.deltaIblt.deltaSerialize();
                partyA.peerCopy.deltaDeserialize(dbB);
                partyB.peerCopy.deltaDeserialize(dbA);
                xferBytes = dbA.size();
                partyA.clearDelta(); partyB.clearDelta();
            }
            mergedA.mergeAdd(partyA.peerCopy);
            auto xferMs = std::chrono::duration_cast<Ms>(Clock::now() - t3).count();

            // 4. Peel (party A's merged copy)
            auto t4 = Clock::now();
            std::set<std::pair<int, int>> start;
            for (int i = 0; i < cfg.k; ++i)
                for (int j = 0; j < ell; ++j)
                    if (mergedA.cell(i, j).count != 0)
                        start.insert({i, j});
            auto [peeled, cascade, ok] = mergedA.incrementalPeel(start);
            auto peelMs = std::chrono::duration_cast<Ms>(Clock::now() - t4).count();

            if (!ok && !peeled.empty())
                std::cerr << "  WARN: peel incomplete ep=" << ep
                          << " peeled=" << peeled.size() << std::endl;

            // 5. Key evolution
            partyA.prfKey = fpsu::prfEvolve(partyA.prfKey.empty() ? "init" : partyA.prfKey, "evolve");
            partyB.prfKey = fpsu::prfEvolve(partyB.prfKey.empty() ? "init" : partyB.prfKey, "evolve");

            auto totalMs = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();

            allOtExt.push_back(otExtMs);
            allPeel.push_back(peelMs);
            if (ep == 0) allEpoch0.push_back(totalMs);
            else {
                allEpochGt0.push_back(totalMs);
                allXfer.push_back(xferBytes);
            }
        }
    }

    auto avg = [](const std::vector<double>& v) {
        if (v.empty()) return 0.0;
        double s = 0; for (auto x : v) s += x; return s / v.size();
    };
    auto avgSz = [](const std::vector<size_t>& v) {
        if (v.empty()) return size_t(0);
        size_t s = 0; for (auto x : v) s += x; return s / v.size();
    };

    size_t fullIbltBytes = ell * cfg.k * 12;
    BenchResult r;
    r.n = cfg.baseN; r.delta = cfg.deltaN;
    r.avgEpoch0Ms  = avg(allEpoch0);
    r.avgEpochGt0Ms = avg(allEpochGt0);
    r.avgOtExtMs   = avg(allOtExt);
    r.avgPeelMs    = avg(allPeel);
    r.avgXferBytes = avgSz(allXfer);
    r.reduction    = allXfer.empty() ? 0.0 : (double)fullIbltBytes / r.avgXferBytes;
    return r;
}

// ============================================================
static void print_header() {
    std::cout << std::string(95, '=') << std::endl
              << std::left
              << std::setw(10) << "n"
              << std::setw(10) << "|Δ|"
              << std::setw(12) << "Epoch0(ms)"
              << std::setw(12) << "Epoch>0(ms)"
              << std::setw(12) << "OT ext(ms)"
              << std::setw(12) << "Peel(ms)"
              << std::setw(12) << "Xfer(B)"
              << std::setw(12) << "Reduction"
              << std::endl << std::string(95, '-') << std::endl;
}

static void print_row(const BenchResult& r) {
    std::cout << std::left << std::fixed << std::setprecision(1)
              << std::setw(10) << r.n
              << std::setw(10) << r.delta
              << std::setw(12) << r.avgEpoch0Ms
              << std::setw(12) << r.avgEpochGt0Ms
              << std::setw(12) << r.avgOtExtMs
              << std::setw(12) << r.avgPeelMs
              << std::setw(12) << r.avgXferBytes
              << std::setw(10) << std::setprecision(1) << r.reduction << "×"
              << std::endl;
}

int main(int argc, char** argv) {
    FullConfig cfg;
    if (argc > 1) cfg.baseN  = std::stoull(argv[1]);
    if (argc > 2) cfg.deltaN = std::stoull(argv[2]);
    if (argc > 3) cfg.lambda = std::stod(argv[3]);
    if (argc > 4) cfg.epochs = std::stoi(argv[4]);
    if (argc > 5) cfg.trials = std::stoi(argv[5]);

    std::cout << "\n  F-PSU Full Protocol Benchmark with OT/OPRF"
              << "\n  OT ext via eval() | IBLT exchange in-process"
              << "\n  n=" << cfg.baseN << "  |Δ|=" << cfg.deltaN
              << "  λ=" << cfg.lambda << "  k=" << cfg.k
              << "  epochs=" << cfg.epochs << "  trials=" << cfg.trials
              << std::endl;

    std::vector<BenchResult> results;
    if (argc > 1) {
        std::cout << "\n  Running n=" << cfg.baseN << " Δ=" << cfg.deltaN
                  << " λ=" << cfg.lambda << " ..." << std::endl;
        auto r = run_one(cfg);
        results.push_back(r);
        std::cout << "  epoch0=" << r.avgEpoch0Ms << "ms  epoch>0="
                  << r.avgEpochGt0Ms << "ms  OT ext=" << r.avgOtExtMs
                  << "ms  peel=" << r.avgPeelMs << "ms" << std::endl;
    } else {
        struct Param { size_t n; size_t delta; double lambda; };
        std::vector<Param> ps = {
            {1024,256,3.0}, {4096,256,3.0}, {16384,1024,3.0},
            {65536,4096,2.5}, {262144,16384,2.0},
        };
        for (auto& p : ps) {
            cfg.baseN=p.n; cfg.deltaN=p.delta; cfg.lambda=p.lambda;
            std::cout << "\n  Running n=" << p.n << " Δ=" << p.delta
                      << " λ=" << p.lambda << " ..." << std::endl;
            auto r = run_one(cfg);
            results.push_back(r);
            std::cout << "  epoch0=" << r.avgEpoch0Ms << "ms  epoch>0="
                      << r.avgEpochGt0Ms << "ms  OT ext=" << r.avgOtExtMs
                      << "ms  peel=" << r.avgPeelMs << "ms" << std::endl;
        }
    }

    std::cout << std::endl;
    print_header();
    for (auto& r : results) print_row(r);
    std::cout << std::string(95, '=') << std::endl;

    std::cout << "\n  OT/OPRF Overhead:\n  "
              << std::left << std::setw(10) << "n"
              << std::setw(12) << "OT ext(%)"
              << std::setw(12) << "Peel(%)"
              << std::setw(14) << "OT+OPRF(%)" << std::endl;
    for (auto& r : results) {
        double op = (r.avgEpochGt0Ms>0) ? 100.0*r.avgOtExtMs/r.avgEpochGt0Ms : 0;
        double pp = (r.avgEpochGt0Ms>0) ? 100.0*r.avgPeelMs/r.avgEpochGt0Ms : 0;
        double tp = (r.avgEpochGt0Ms>0) ? 200.0*r.avgOtExtMs/r.avgEpochGt0Ms : 0;
        std::cout << "  " << std::left << std::fixed << std::setprecision(1)
                  << std::setw(10) << r.n
                  << std::setw(12) << op
                  << std::setw(12) << pp
                  << std::setw(14) << tp << std::endl;
    }
    std::cout << "\n  KOS OT throughput (from OT ext column):" << std::endl;
    for (auto& r : results) {
        size_t nOT = 2 * cfg.k * r.delta;
        double tput = (r.avgOtExtMs > 0) ? nOT / r.avgOtExtMs * 1000 : 0;
        std::cout << "    n=" << r.n << ": " << nOT << " OTs in "
                  << r.avgOtExtMs << "ms = "
                  << std::fixed << std::setprecision(1)
                  << (tput/1e6) << "M OTs/s" << std::endl;
    }

    return 0;
}
