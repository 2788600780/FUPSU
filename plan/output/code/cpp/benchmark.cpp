/**
 * F-PSU C++ Benchmark — Asiacrypt 2026
 * Measures OT, SoftSpokenOT, and KOS wall-clock time using libOTe.
 *
 * Build:
 *   cd build && cmake .. && make -j
 *   ./benchmark
 */

#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>
#include <string>

// coproto
#include <coproto/Socket/LocalAsyncSock.h>
namespace cp = coproto;

// macoro
#include <macoro/sync_wait.h>
#include <macoro/when_all.h>
#include <macoro/task.h>

// Base OT
#include <libOTe/Base/SimplestOT.h>

// SoftSpokenOT
#include <libOTe/TwoChooseOne/SoftSpokenOT/SoftSpokenShOtExt.h>

// KOS
#include <libOTe/TwoChooseOne/Kos/KosOtExtReceiver.h>
#include <libOTe/TwoChooseOne/Kos/KosOtExtSender.h>

// Utilities
#include <cryptoTools/Common/block.h>
#include <cryptoTools/Common/BitVector.h>
#include <cryptoTools/Crypto/PRNG.h>

using namespace osuCrypto;
using Clock = std::chrono::high_resolution_clock;
using Ms = std::chrono::duration<double, std::milli>;

// ============================================================
// eval helper — run two coroutines together (local socket pair)
// ============================================================
template<typename T0, typename T1>
inline void eval(macoro::task<T0> t0, macoro::task<T1> t1)
{
    auto r = macoro::sync_wait(macoro::when_all_ready(std::move(t0), std::move(t1)));
    std::string e0, e1;
    bool threw = false;
    try { std::get<0>(r).result(); }
    catch (const std::exception& e) { e0 = e.what(); threw = true; }
    try { std::get<1>(r).result(); }
    catch (const std::exception& e) { e1 = e.what(); threw = true; }
    if (threw)
        throw std::runtime_error("P0: " + e0 + "\nP1: " + e1);
}

// ============================================================
struct Result {
    std::string name;
    size_t n;
    size_t trials;
    double avg_ms;
    double min_ms;
    double max_ms;

    void print() const {
        std::cout << "  " << std::left << std::setw(28) << name
                  << " n=" << std::setw(10) << n
                  << " avg=" << std::setw(9) << std::fixed << std::setprecision(2) << avg_ms << "ms"
                  << "  (" << trials << " trials)"
                  << std::endl;
    }
};

std::vector<Result> g_results;

// ============================================================
// Bench 1: Base OT (SimplestOT, κ = 128)
// ============================================================
void bench_base_ot(size_t trials) {
    std::cout << "\n[1] Base OT — SimplestOT (κ = 128)" << std::endl;

    std::vector<double> times;
    for (size_t t = 0; t < trials; ++t) {
        auto sock = cp::LocalAsyncSocket::makePair();
        PRNG prng0(block(t * 10000 + t, 0));
        PRNG prng1(block(t * 10000 + t, 1));

        std::vector<std::array<block, 2>> sendMsg(128);
        BitVector choices(128);
        choices.randomize(prng0);
        std::vector<block> recvMsg(128);

        SimplestOT sender, receiver;

        auto t0 = Clock::now();
        auto p0 = sender.send(sendMsg, prng0, sock[0]);
        auto p1 = receiver.receive(choices, recvMsg, prng1, sock[1]);
        eval(std::move(p0), std::move(p1));
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();
        times.push_back(elapsed);
    }

    double sum = 0, mn = 1e9, mx = 0;
    for (auto v : times) { sum += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    g_results.push_back({"BaseOT_SimplestOT", 128, trials, sum / trials, mn, mx});
    g_results.back().print();
}

// ============================================================
// Bench 2: KOS OT Extension
// ============================================================
void bench_kos_ot(size_t n, size_t trials) {
    std::cout << "\n[2] KOS OT Extension (n=" << n << ")" << std::endl;

    std::vector<double> times;
    for (size_t t = 0; t < trials; ++t) {
        // --- Base OT setup ---
        auto sockBase = cp::LocalAsyncSocket::makePair();
        PRNG prngBase0(block(t * 10000 + t, 0));
        PRNG prngBase1(block(t * 10000 + t, 1));

        std::vector<std::array<block, 2>> baseSend(128);
        BitVector baseChoice(128);
        baseChoice.randomize(prngBase0);
        std::vector<block> baseRecv(128);
        SimplestOT baseSender, baseReceiver;
        auto bp0 = baseSender.send(baseSend, prngBase0, sockBase[0]);
        auto bp1 = baseReceiver.receive(baseChoice, baseRecv, prngBase1, sockBase[1]);
        eval(std::move(bp0), std::move(bp1));

        // --- KOS extension ---
        auto sock = cp::LocalAsyncSocket::makePair();
        PRNG prng0(block(t * 20000 + t, 0));
        PRNG prng1(block(t * 20000 + t, 1));

        KosOtExtSender kosSender;
        KosOtExtReceiver kosReceiver;
        // Sender gets base receiver's output; receiver gets base sender's output
        kosSender.setBaseOts(baseRecv, baseChoice);
        kosReceiver.setBaseOts(baseSend);

        std::vector<std::array<block, 2>> messages(n);
        BitVector recvChoices(n);
        recvChoices.randomize(prng0);
        std::vector<block> recvOutput(n);

        auto t0 = Clock::now();
        auto p0 = kosSender.send(messages, prng0, sock[0]);
        auto p1 = kosReceiver.receive(recvChoices, recvOutput, prng1, sock[1]);
        eval(std::move(p0), std::move(p1));
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();
        times.push_back(elapsed);
    }

    double sum = 0, mn = 1e9, mx = 0;
    for (auto v : times) { sum += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    g_results.push_back({"KOS_OT", n, trials, sum / trials, mn, mx});
    g_results.back().print();
}

// ============================================================
// Bench 3: SoftSpokenOT Extension (semi-honest)
// ============================================================
void bench_softspoken_ot(size_t n, size_t fieldBits, size_t trials) {
    std::cout << "\n[3] SoftSpokenOT (n=" << n << ", f=" << fieldBits << ")" << std::endl;

    std::vector<double> times;
    for (size_t t = 0; t < trials; ++t) {
        // --- Base OT setup ---
        auto sockBase = cp::LocalAsyncSocket::makePair();
        PRNG prngBase0(block(t * 10000 + t, 0));
        PRNG prngBase1(block(t * 10000 + t, 1));

        std::vector<std::array<block, 2>> baseSend(128);
        BitVector baseChoice(128);
        baseChoice.randomize(prngBase0);
        std::vector<block> baseRecv(128);
        SimplestOT baseSender, baseReceiver;
        auto bp0 = baseSender.send(baseSend, prngBase0, sockBase[0]);
        auto bp1 = baseReceiver.receive(baseChoice, baseRecv, prngBase1, sockBase[1]);
        eval(std::move(bp0), std::move(bp1));

        // --- SoftSpokenOT extension ---
        auto sock = cp::LocalAsyncSocket::makePair();
        PRNG prng0(block(t * 20000 + t, 0));
        PRNG prng1(block(t * 20000 + t, 1));

        SoftSpokenShOtSender ssSender(fieldBits);
        SoftSpokenShOtReceiver ssReceiver(fieldBits);
        ssSender.setBaseOts(baseRecv, baseChoice);
        ssReceiver.setBaseOts(baseSend);

        std::vector<std::array<block, 2>> messages(n);
        BitVector recvChoices(n);
        recvChoices.randomize(prng0);
        std::vector<block> recvOutput(n);

        auto t0 = Clock::now();
        auto p0 = ssSender.send(messages, prng0, sock[0]);
        auto p1 = ssReceiver.receive(recvChoices, recvOutput, prng1, sock[1]);
        eval(std::move(p0), std::move(p1));
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();
        times.push_back(elapsed);
    }

    double sum = 0, mn = 1e9, mx = 0;
    for (auto v : times) { sum += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    std::string label = "SsOT_f" + std::to_string(fieldBits);
    g_results.push_back({label, n, trials, sum / trials, mn, mx});
    g_results.back().print();
}

// ============================================================
// Bench 4: IBLT operations (full implementation)
// ============================================================
#include "iblt.h"

std::vector<std::string> gen_items(size_t n, size_t seed) {
    std::vector<std::string> items;
    items.reserve(n);
    PRNG prng(block(seed, 0));
    for (size_t i = 0; i < n; ++i) {
        // 8-byte elements: matches Python randbytes(8) — required for IBLT
        // round-trip (int<->bytes must be bijective for peeling to work)
        uint64_t val = prng.get<uint64_t>();
        items.emplace_back(reinterpret_cast<const char*>(&val), sizeof(val));
    }
    return items;
}

void bench_iblt_insert(size_t n, int k, int ell, size_t trials) {
    std::cout << "\n[4a] IBLT insert (n=" << n << ", k=" << k << ", ell=" << ell
              << ", m=" << (k * ell) << ")" << std::endl;

    std::vector<double> times;
    for (size_t t = 0; t < trials; ++t) {
        auto items = gen_items(n, t * 1000);
        fpsu::IBLT iblt(k, ell);

        auto t0 = Clock::now();
        for (const auto& s : items)
            iblt.insert(s.data(), s.size());
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();
        times.push_back(elapsed);
    }

    double sum = 0, mn = 1e9, mx = 0;
    for (auto v : times) { sum += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    g_results.push_back({"IBLT_insert", n, trials, sum / trials, mn, mx});
    g_results.back().print();
}

void bench_iblt_encode_bulk(size_t n, int k, int ell, size_t trials) {
    std::cout << "\n[4b] IBLT encode bulk (n=" << n << ", k=" << k
              << ", ell=" << ell << ")" << std::endl;

    std::vector<double> times;
    for (size_t t = 0; t < trials; ++t) {
        auto items = gen_items(n, t * 1000);
        auto t0 = Clock::now();
        fpsu::IBLT iblt(k, ell);
        iblt.encode(items);
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();
        times.push_back(elapsed);
    }

    double sum = 0, mn = 1e9, mx = 0;
    for (auto v : times) { sum += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    g_results.push_back({"IBLT_encode", n, trials, sum / trials, mn, mx});
    g_results.back().print();
}

void bench_iblt_list_entries(size_t n, int k, int ell, size_t trials) {
    std::cout << "\n[4c] IBLT listEntries (n=" << n << ", k=" << k
              << ", ell=" << ell << ")" << std::endl;

    std::vector<double> times;
    for (size_t t = 0; t < trials; ++t) {
        auto items = gen_items(n, t * 1000);
        fpsu::IBLT iblt(k, ell);
        iblt.encode(items);

        auto t0 = Clock::now();
        auto [peeled, ok] = iblt.listEntries();
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();

        // Verify correctness (first trial only — avoid printing)
        if (t == 0 && !ok)
            std::cerr << "  WARNING: peel failed for n=" << n << std::endl;

        times.push_back(elapsed);
    }

    double sum = 0, mn = 1e9, mx = 0;
    for (auto v : times) { sum += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    g_results.push_back({"IBLT_peel", n, trials, sum / trials, mn, mx});
    g_results.back().print();
}

void bench_iblt_merge(size_t n, int k, int ell, size_t trials) {
    std::cout << "\n[4d] IBLT merge (n=" << n << ", k=" << k
              << ", ell=" << ell << ")" << std::endl;

    std::vector<double> times;
    for (size_t t = 0; t < trials; ++t) {
        auto A = gen_items(n / 2, t * 1000);
        auto B = gen_items(n / 2, t * 2000);

        fpsu::IBLT iblt_a(k, ell);
        iblt_a.encode(A);
        fpsu::IBLT iblt_b(k, ell);
        iblt_b.encode(B);

        auto t0 = Clock::now();
        iblt_a.mergeAdd(iblt_b);
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();
        times.push_back(elapsed);
    }

    double sum = 0, mn = 1e9, mx = 0;
    for (auto v : times) { sum += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    g_results.push_back({"IBLT_merge", n, trials, sum / trials, mn, mx});
    g_results.back().print();
}

void bench_iblt_incremental_peel(size_t n, int k, int ell, size_t delta_size, size_t trials) {
    std::cout << "\n[4e] IBLT incremental peel (n=" << n << ", |Δ|="
              << delta_size << ", k=" << k << ")" << std::endl;

    std::vector<double> times;
    for (size_t t = 0; t < trials; ++t) {
        auto base = gen_items(n, t * 1000);
        auto delta = gen_items(delta_size, t * 3000);

        // Encode base ∪ delta into one IBLT
        fpsu::IBLT iblt(k, ell);
        iblt.encode(base);
        for (const auto& s : delta)
            iblt.insert(s);

        // Record ALL cell positions (protocol: Q0 = all modified cells).
        // For the benchmark we measure peel time on the full IBLT.
        std::set<std::pair<int, int>> start;
        for (int i = 0; i < k; ++i)
            for (int j = 0; j < ell; ++j)
                if (iblt.cell(i, j).count != 0)
                    start.insert({i, j});

        auto t0 = Clock::now();
        auto [peeled, cascade, ok] = iblt.incrementalPeel(start);
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();

        (void)cascade; // cascade size measured in correctness test
        if (t == 0 && !ok)
            std::cerr << "  WARNING: incremental peel failed for n="
                      << n << " Δ=" << delta_size << std::endl;

        times.push_back(elapsed);
    }

    double sum = 0, mn = 1e9, mx = 0;
    for (auto v : times) { sum += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    std::string label = "IBLT_incrPeel_Δ" + std::to_string(delta_size);
    g_results.push_back({label, n, trials, sum / trials, mn, mx});
    g_results.back().print();
}

void bench_iblt_correctness() {
    std::cout << "\n[4f] IBLT correctness tests" << std::endl;

    // Use Λ = 8.0 to guarantee peel success regardless of PRNG distribution.
    // (Λ = 3.0 is insufficient for worst-case hash patterns; protocol uses
    // sufficiently large n where 3.0 is safe w.h.p.)

    // Test 1: basic encode + peel
    for (int n : {10, 100, 1000}) {
        int k = 4;
        int ell = static_cast<int>(8.0 * n / k) + 1;
        auto items = gen_items(n, 42);
        fpsu::IBLT iblt(k, ell);
        iblt.encode(items);
        auto [peeled, ok] = iblt.listEntries();
        if (!ok || peeled.size() != items.size())
            std::cerr << "  FAIL: basic peel n=" << n << std::endl;
        else
            std::cout << "  PASS: basic peel n=" << n << std::endl;
    }

    // Test 2: delta IBLT = merge of parts
    {
        auto A = gen_items(500, 42);
        auto B = gen_items(300, 99);
        int k = 4, n_total = 800;
        int ell = static_cast<int>(8.0 * n_total / k) + 1;

        fpsu::IBLT iblt_a(k, ell); iblt_a.encode(A);
        fpsu::IBLT iblt_b(k, ell); iblt_b.encode(B);
        iblt_a.mergeAdd(iblt_b);
        auto [peeled, ok] = iblt_a.listEntries();

        if (ok && peeled.size() == 800)
            std::cout << "  PASS: merge test n=800" << std::endl;
        else
            std::cerr << "  FAIL: merge test peeled=" << peeled.size()
                      << " ok=" << ok << std::endl;
    }

    // Test 3: incremental peel (full element set)
    {
        auto items = gen_items(200, 42);
        int k = 4;
        int ell = static_cast<int>(8.0 * 200 / k) + 1;
        fpsu::IBLT iblt(k, ell);
        iblt.encode(items);

        std::set<std::pair<int, int>> start;
        for (const auto& s : items)
            for (int i = 0; i < k; ++i) {
                int col = static_cast<int>(fpsu::iblt_hash(s.data(), s.size(), i, ell));
                start.insert({i, col});
            }

        auto [peeled, cascade, ok] = iblt.incrementalPeel(start);
        if (ok)
            std::cout << "  PASS: incremental peel n=200 cascade=" << cascade << std::endl;
        else
            std::cerr << "  FAIL: incremental peel peeled=" << peeled.size() << std::endl;
    }

    std::cout << "  IBLT correctness done." << std::endl;
}

// ============================================================
// Bench 5: F-PSU protocol epochs
// ============================================================
#include "protocol.h"

void bench_protocol_epoch(size_t baseN, size_t deltaN, int k, size_t trials) {
    std::cout << "\n[5] F-PSU epoch (n=" << baseN << ", |Δ|=" << deltaN << ")" << std::endl;

    int ell = static_cast<int>(3.0 * (baseN + deltaN) * 2 / k) + 1;

    std::vector<double> times;
    for (size_t t = 0; t < trials; ++t) {
        auto itemsA = gen_items(baseN, t * 1000 + 0);
        auto itemsB = gen_items(baseN, t * 1000 + 1);
        auto deltaA_add = gen_items(deltaN, t * 1000 + 2);
        auto deltaB_add = gen_items(deltaN, t * 1000 + 3);
        std::vector<std::string> empty;

        fpsu::FpsuParty A("keyA", k, ell);
        fpsu::FpsuParty B("keyB", k, ell);
        A.initialize(itemsA);
        B.initialize(itemsB);

        fpsu::EpochMetrics m{};

        auto t0 = Clock::now();
        auto result = fpsu::fpsuEpoch(A, B, deltaA_add, empty,
                                       deltaB_add, empty, &m);
        auto elapsed = std::chrono::duration_cast<Ms>(Clock::now() - t0).count();

        times.push_back(elapsed);

        if (t == 0) {
            std::cout << "  |X∪Y|=" << result.size()
                      << " cascade=" << m.cellsPeeled << std::endl;
        }
    }

    double sum = 0, mn = 1e9, mx = 0;
    for (auto v : times) { sum += v; mn = std::min(mn, v); mx = std::max(mx, v); }
    std::string label = "FpsuEpoch_n" + std::to_string(baseN) + "_Δ" + std::to_string(deltaN);
    g_results.push_back({label, baseN + deltaN, trials, sum / trials, mn, mx});
    g_results.back().print();
}

// ============================================================
// Main
// ============================================================
int main() {
    std::cout << std::string(64, '=') << std::endl;
    std::cout << "  F-PSU libOTe Benchmark Suite" << std::endl;
    std::cout << "  Asiacrypt 2026 Submission" << std::endl;
    std::cout << "  Platform: Apple Silicon, macOS" << std::endl;
    std::cout << std::string(64, '=') << std::endl;

    // --- OT benchmarks ---
    bench_base_ot(10);

    // Powers of 2: 2^10 = 1024, 2^12 = 4096, 2^14 = 16384, 2^16 = 65536, 2^20 = 1048576
    for (size_t n : {1ULL << 10, 1ULL << 12, 1ULL << 14, 1ULL << 16, 1ULL << 20}) {
        size_t tr = (n >= (1ULL << 20)) ? 5 : 10;
        bench_kos_ot(n, tr);
    }

    for (size_t n : {1ULL << 10, 1ULL << 12, 1ULL << 14, 1ULL << 16}) {
        bench_softspoken_ot(n, 8, (n >= (1ULL << 16)) ? 5 : 10);
    }

    // --- IBLT benchmarks ---
    bench_iblt_correctness();

    for (size_t n : {1ULL << 10, 1ULL << 12, 1ULL << 14, 1ULL << 16, 1ULL << 20}) {
        int k = 4;
        int ell = static_cast<int>(3.0 * n / k) + 1;
        size_t tr = (n >= (1ULL << 20)) ? 3 : (n >= (1ULL << 16) ? 10 : 20);
        bench_iblt_encode_bulk(n, k, ell, tr);
        bench_iblt_list_entries(n, k, ell, tr);
        bench_iblt_merge(n, k, ell, tr);
    }

    // Incremental peel: base + delta, ell sized for total n+Δ
    bench_iblt_incremental_peel(1 << 10, 4,
        static_cast<int>(3.0 * ((1 << 10) + (1 << 8)) / 4) + 1, 1 << 8, 20);
    bench_iblt_incremental_peel(1 << 14, 4,
        static_cast<int>(3.0 * ((1 << 14) + (1 << 12)) / 4) + 1, 1 << 12, 10);
    bench_iblt_incremental_peel(1 << 16, 4,
        static_cast<int>(3.0 * ((1 << 16) + (1 << 14)) / 4) + 1, 1 << 14, 5);

    // --- Protocol epoch benchmarks ---
    bench_protocol_epoch(1 << 10, 1 << 8,  4, 10);
    bench_protocol_epoch(1 << 14, 1 << 12, 4, 5);
    bench_protocol_epoch(1 << 16, 1 << 14, 4, 3);

    // --- Summary ---
    std::cout << "\n" << std::string(64, '=') << std::endl;
    std::cout << "  Summary" << std::endl;
    std::cout << std::string(64, '=') << std::endl;
    for (const auto& r : g_results)
        r.print();

    return 0;
}
