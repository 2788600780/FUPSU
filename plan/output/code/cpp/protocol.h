#pragma once
/**
 * F-PSU Protocol Orchestration — C++ reference implementation.
 * Asiacrypt 2026.
 *
 * Implements Π_fpsu: Forward-Private Updatable PSU.
 * Three rounds per epoch:
 *   1. Local IBLT update (delta encode + merge)
 *   2. UnionPeel (IBLT-based set union recovery)
 *   3. Key evolution (PRF chain)
 */

#include "iblt.h"

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace fpsu {

/// Per-epoch metrics for the protocol.
struct EpochMetrics {
    int    epoch;
    size_t setSizeA;        // |X_t|
    size_t setSizeB;        // |Y_t|
    size_t deltaSize;       // |ΔX| + |ΔY|
    size_t unionSize;       // |X ∪ Y|
    size_t cellsPeeled;      // cascade size
    double encodeMs;         // Round 1 time
    double peelMs;           // Round 2 time
};

/// F-PSU protocol instance (one party).
class FpsuParty {
public:
    FpsuParty(const std::string& prfKey, int k, int ellMax);

    /// Initialize with initial set.
    void initialize(const std::vector<std::string>& items);

    /// Process one epoch with deltas.
    /// Returns the current set union after this epoch.
    std::unordered_set<std::string> epoch(
        const std::vector<std::string>& addDelta,
        const std::vector<std::string>& removeDelta,
        const IBLT& peerIblt,
        EpochMetrics* metrics = nullptr);

    /// Get current IBLT (for UnionPeel exchange).
    const IBLT& iblt() const { return iblt_; }
    IBLT& iblt() { return iblt_; }

    /// Get current PRF key (for key evolution).
    const std::string& prfKey() const { return prfKey_; }

private:
    std::string prfKey_;
    IBLT        iblt_;
    int         k_;
};

/// Simulate one epoch of the F-PSU protocol between two parties.
/// Both parties know their own deltas and exchange IBLTs.
/// Returns the union of both sets.
std::unordered_set<std::string> fpsuEpoch(
    FpsuParty& partyA,
    FpsuParty& partyB,
    const std::vector<std::string>& deltaA_add,
    const std::vector<std::string>& deltaA_rem,
    const std::vector<std::string>& deltaB_add,
    const std::vector<std::string>& deltaB_rem,
    EpochMetrics* metrics = nullptr);

/// PRF chain: K_t = SHA256(K_{t-1} || label)
std::string prfEvolve(const std::string& key, const std::string& label);

} // namespace fpsu
