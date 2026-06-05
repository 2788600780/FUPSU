#pragma once
/**
 * F-PSU IBLT — C++ port of the Python reference implementation.
 * Asiacrypt 2026.
 *
 * Invertible Bloom Lookup Table with delta encoding and incremental peeling.
 * Reference: Piske & Trieu, "Is PSI Really Faster Than PSU?", Eurocrypt 2026.
 */

#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

namespace fpsu {

struct IBltCell {
    uint64_t sum;   // mod-M sum of element IDs
    int32_t  count; // elements hashed to this cell
};

/// SHA-256 based hash: (subtable_index || element) -> position in [0, width).
uint64_t iblt_hash(const void* data, size_t len, int subtable, int width);

class IBLT {
public:
    // M_ = 0 → natural uint64 wrap (mod 2^64, matches Python default).
    IBLT(int k, int ell, uint64_t M = 0);

    int k()   const { return k_; }
    int ell() const { return ell_; }
    int m()   const { return m_; }

    // ---- Element operations ----
    void insert(const void* data, size_t len);
    void insert(const std::string& s) { insert(s.data(), s.size()); }
    void remove(const void* data, size_t len);
    void remove(const std::string& s) { remove(s.data(), s.size()); }

    void encode(const std::vector<std::string>& items);

    /// Full peel: returns (recovered_elements, success).
    std::pair<std::unordered_set<std::string>, bool> listEntries();

    // ---- Delta IBLT (linearity) ----
    void mergeAdd(const IBLT& other);
    void mergeSub(const IBLT& other);

    /// Positions of all cells with non-zero count.
    std::set<std::pair<int, int>> modifiedCells() const;

    /// Peel starting from given cells. Returns (peeled, cascade_size, success).
    std::tuple<std::unordered_set<std::string>, int, bool>
    incrementalPeel(const std::set<std::pair<int, int>>& startCells);

    // ---- Accessors ----
    const IBltCell& cell(int i, int j) const { return cells_[i * ell_ + j]; }
    IBltCell& cell(int i, int j) { return cells_[i * ell_ + j]; }

    // ---- Queries ----
    bool isEmpty() const;
    int  nonZeroCells() const;
    size_t serializedBytes() const { return m_ * 12; }

    // ---- Network serialization ----
    std::vector<uint8_t> serialize() const;
    void deserialize(const std::vector<uint8_t>& data);

    // ---- Sparse delta exchange (O(k|Δ|) communication) ----
    std::vector<uint8_t> deltaSerialize() const;
    void deltaDeserialize(const std::vector<uint8_t>& data);

    // Return modified cells that a delta buffer would touch (for start-set)
    static std::set<std::pair<int,int>> deltaCells(
        const std::vector<uint8_t>& data);

private:
    void hashPositions(const void* data, size_t len,
                       int positions[][2]) const;

    int      k_;
    int      ell_;
    int      m_;
    uint64_t M_;
    std::vector<IBltCell> cells_;
};

} // namespace fpsu
