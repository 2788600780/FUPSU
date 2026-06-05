/**
 * F-PSU Protocol Orchestration — C++ implementation.
 * Asiacrypt 2026.
 */
#include "protocol.h"

#include <openssl/sha.h>
#include <chrono>
#include <iostream>

namespace fpsu {

using Clock = std::chrono::high_resolution_clock;
using Ms = std::chrono::duration<double, std::milli>;

// ============================================================
// PRF chain
// ============================================================

std::string prfEvolve(const std::string& key, const std::string& label) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, key.data(), key.size());
    SHA256_Update(&ctx, label.data(), label.size());
    std::string out(SHA256_DIGEST_LENGTH, '\0');
    SHA256_Final((unsigned char*)out.data(), &ctx);
    return out;
}

// ============================================================
// FpsuParty
// ============================================================

FpsuParty::FpsuParty(const std::string& prfKey, int k, int ellMax)
    : prfKey_(prfKey), iblt_(k, ellMax), k_(k) {}

void FpsuParty::initialize(const std::vector<std::string>& items) {
    iblt_.encode(items);
}

std::unordered_set<std::string> FpsuParty::epoch(
    const std::vector<std::string>& addDelta,
    const std::vector<std::string>& removeDelta,
    const IBLT& peerIblt,
    EpochMetrics* metrics) {

    auto t0 = Clock::now();

    // ---- Round 1: Local IBLT update ----
    // Add new elements
    for (const auto& x : addDelta)
        iblt_.insert(x);
    // Remove deleted elements
    for (const auto& x : removeDelta)
        iblt_.remove(x);

    auto t1 = Clock::now();

    // ---- Round 2: UnionPeel ----
    // Merge peer's IBLT into local copy (simulating IBLT exchange)
    IBLT merged = iblt_;
    merged.mergeAdd(peerIblt);

    // Collect all modified cells as starting points
    std::set<std::pair<int, int>> startCells;
    for (int i = 0; i < merged.k(); ++i)
        for (int j = 0; j < merged.ell(); ++j)
            if (merged.cell(i, j).count != 0)
                startCells.insert({i, j});

    auto [peeled, cascade, ok] = merged.incrementalPeel(startCells);

    if (!ok && !peeled.empty())
        std::cerr << "  Warning: UnionPeel incomplete ("
                  << peeled.size() << " peeled)" << std::endl;

    auto t2 = Clock::now();

    // ---- Round 3: Key evolution ----
    // New key = PRF(old_key, "evolve")
    prfKey_ = prfEvolve(prfKey_, "evolve");

    // Record metrics
    if (metrics) {
        metrics->cellsPeeled = cascade;
        metrics->encodeMs = std::chrono::duration_cast<Ms>(t1 - t0).count();
        metrics->peelMs   = std::chrono::duration_cast<Ms>(t2 - t1).count();
        metrics->deltaSize = addDelta.size() + removeDelta.size();
        metrics->unionSize = peeled.size();
    }

    return peeled;
}

// ============================================================
// fpsuEpoch — two-party simulation
// ============================================================

std::unordered_set<std::string> fpsuEpoch(
    FpsuParty& partyA,
    FpsuParty& partyB,
    const std::vector<std::string>& deltaA_add,
    const std::vector<std::string>& deltaA_rem,
    const std::vector<std::string>& deltaB_add,
    const std::vector<std::string>& deltaB_rem,
    EpochMetrics* metrics) {

    // Each party updates its IBLT locally
    IBLT ibltA_before = partyA.iblt();
    IBLT ibltB_before = partyB.iblt();

    // Simulate both parties applying deltas and exchanging IBLTs
    // Party A updates with its deltas
    for (const auto& x : deltaA_add)
        partyA.iblt().insert(x);
    for (const auto& x : deltaA_rem)
        partyA.iblt().remove(x);

    // Party B updates with its deltas
    for (const auto& x : deltaB_add)
        partyB.iblt().insert(x);
    for (const auto& x : deltaB_rem)
        partyB.iblt().remove(x);

    // Merge both IBLTs for UnionPeel
    IBLT merged = partyA.iblt();
    merged.mergeAdd(partyB.iblt());

    // Peel all
    std::set<std::pair<int, int>> start;
    for (int i = 0; i < merged.k(); ++i)
        for (int j = 0; j < merged.ell(); ++j)
            if (merged.cell(i, j).count != 0)
                start.insert({i, j});

    auto [peeled, cascade, ok] = merged.incrementalPeel(start);

    if (metrics) {
        metrics->cellsPeeled = cascade;
        metrics->unionSize = peeled.size();
        metrics->deltaSize = deltaA_add.size() + deltaA_rem.size()
                           + deltaB_add.size() + deltaB_rem.size();
    }

    return peeled;
}

} // namespace fpsu
