/**
 * F-PSU IBLT — C++ implementation.
 * Asiacrypt 2026.
 */
#include "iblt.h"

#include <openssl/sha.h>
#include <cassert>
#include <cstring>
#include <queue>
#include <deque>

namespace fpsu {

// SHA-256((subtable_index as uint32 big-endian) || data) → hash, mod width.
uint64_t iblt_hash(const void* data, size_t len, int subtable, int width) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    uint8_t idx_be[4];
    idx_be[0] = (subtable >> 24) & 0xFF;
    idx_be[1] = (subtable >> 16) & 0xFF;
    idx_be[2] = (subtable >>  8) & 0xFF;
    idx_be[3] =  subtable        & 0xFF;
    SHA256_Update(&ctx, idx_be, 4);
    SHA256_Update(&ctx, data, len);

    uint8_t out[SHA256_DIGEST_LENGTH];
    SHA256_Final(out, &ctx);

    // First 8 bytes as uint64, mod width
    uint64_t h = 0;
    for (int i = 0; i < 8; ++i)
        h = (h << 8) | out[i];
    return h % width;
}

// ============================================================
// IBLT
// ============================================================

// Element ID: first 8 bytes as native-endian uint64.
// Must be consistent with peel reconstruction (which also uses native
// endian) so that hash(find_positions(original_bytes)) ==
// hash(find_positions(reconstructed_bytes)).
static uint64_t elem_id(const void* data, size_t len) {
    uint64_t v = 0;
    size_t n = std::min(len, (size_t)8);
    memcpy(&v, data, n);
    return v;
}

IBLT::IBLT(int k, int ell, uint64_t M)
    : k_(k), ell_(ell), m_(k * ell), M_(M) {
    cells_.resize(m_, {0, 0});
}

void IBLT::hashPositions(const void* data, size_t len,
                         int positions[][2]) const {
    for (int i = 0; i < k_; ++i) {
        positions[i][0] = i;
        positions[i][1] = static_cast<int>(iblt_hash(data, len, i, ell_));
    }
}

// ---- Element Operations ----

void IBLT::insert(const void* data, size_t len) {
    uint64_t eid = elem_id(data, len);
    if (M_ != 0) eid %= M_;

    int positions[8][2];
    hashPositions(data, len, positions);
    for (int i = 0; i < k_; ++i) {
        int row = positions[i][0];
        int col = positions[i][1];
        IBltCell& c = cell(row, col);
        if (M_ != 0) c.sum = (c.sum + eid) % M_;
        else         c.sum += eid;
        c.count += 1;
    }
}

void IBLT::remove(const void* data, size_t len) {
    uint64_t eid = elem_id(data, len);
    if (M_ != 0) eid %= M_;

    int positions[8][2];
    hashPositions(data, len, positions);
    for (int i = 0; i < k_; ++i) {
        int row = positions[i][0];
        int col = positions[i][1];
        IBltCell& c = cell(row, col);
        if (M_ != 0) c.sum = (c.sum - eid + M_) % M_;
        else         c.sum -= eid;
        c.count -= 1;
    }
}

void IBLT::encode(const std::vector<std::string>& items) {
    for (const auto& s : items)
        insert(s.data(), s.size());
}

// ---- Peeling ----

std::pair<std::unordered_set<std::string>, bool> IBLT::listEntries() {
    std::unordered_set<std::string> recovered;

    // Build initial queue of all cells
    std::queue<std::pair<int,int>> Q;
    for (int i = 0; i < k_; ++i)
        for (int j = 0; j < ell_; ++j)
            Q.push({i, j});

    while (!Q.empty()) {
        std::unordered_set<std::string> newly_peeled;

        size_t qs = Q.size();
        for (size_t t = 0; t < qs; ++t) {
            auto [i, j] = Q.front();
            Q.pop();
            if (cell(i, j).count == 1) {
                uint64_t x_int = cell(i, j).sum;
                std::string x_bytes(reinterpret_cast<const char*>(&x_int), 8);
                newly_peeled.insert(x_bytes);
            }
        }

        if (newly_peeled.empty())
            break;

        for (const auto& s : newly_peeled) {
            recovered.insert(s);
            remove(s.data(), s.size());
        }

        // Push cells affected by removed elements
        for (const auto& x : newly_peeled) {
            int positions[8][2];
            hashPositions(x.data(), x.size(), positions);
            for (int i = 0; i < k_; ++i)
                Q.push({positions[i][0], positions[i][1]});
        }
    }

    bool success = isEmpty();
    return {recovered, success};
}

// ---- Delta IBLT ----

void IBLT::mergeAdd(const IBLT& other) {
    assert(k_ == other.k_ && ell_ == other.ell_);
    for (int i = 0; i < m_; ++i) {
        if (M_ != 0) cells_[i].sum = (cells_[i].sum + other.cells_[i].sum) % M_;
        else         cells_[i].sum += other.cells_[i].sum;
        cells_[i].count += other.cells_[i].count;
    }
}

void IBLT::mergeSub(const IBLT& other) {
    assert(k_ == other.k_ && ell_ == other.ell_);
    for (int i = 0; i < m_; ++i) {
        if (M_ != 0) cells_[i].sum = (cells_[i].sum - other.cells_[i].sum + M_) % M_;
        else         cells_[i].sum -= other.cells_[i].sum;
        cells_[i].count -= other.cells_[i].count;
    }
}

std::set<std::pair<int, int>> IBLT::modifiedCells() const {
    std::set<std::pair<int, int>> result;
    for (int i = 0; i < k_; ++i)
        for (int j = 0; j < ell_; ++j)
            if (cell(i, j).count != 0)
                result.insert({i, j});
    return result;
}

std::tuple<std::unordered_set<std::string>, int, bool>
IBLT::incrementalPeel(const std::set<std::pair<int, int>>& startCells) {
    std::unordered_set<std::string> peeled;
    std::queue<std::pair<int, int>> Q;
    for (auto& p : startCells)
        Q.push(p);
    int cascade = static_cast<int>(startCells.size());

    while (!Q.empty()) {
        std::unordered_set<std::string> newly;

        size_t qs = Q.size();
        for (size_t t = 0; t < qs; ++t) {
            auto [i, j] = Q.front();
            Q.pop();
            if (cell(i, j).count == 1) {
                uint64_t x_int = cell(i, j).sum;
                std::string x_bytes(reinterpret_cast<const char*>(&x_int), 8);
                newly.insert(x_bytes);
            }
        }

        if (newly.empty())
            break;

        for (const auto& s : newly) {
            peeled.insert(s);
            remove(s.data(), s.size());
        }

        for (const auto& x : newly) {
            int positions[8][2];
            hashPositions(x.data(), x.size(), positions);
            for (int i = 0; i < k_; ++i) {
                Q.push({positions[i][0], positions[i][1]});
                ++cascade;
            }
        }
    }

    bool success = isEmpty();
    return {peeled, cascade, success};
}

// ---- Serialization ----

std::vector<uint8_t> IBLT::serialize() const {
    std::vector<uint8_t> buf(m_ * 12);
    for (int i = 0; i < m_; ++i) {
        uint8_t* p = buf.data() + i * 12;
        uint64_t sum = cells_[i].sum;
        int32_t  cnt = cells_[i].count;
        memcpy(p,     &sum, 8);
        memcpy(p + 8, &cnt, 4);
    }
    return buf;
}

void IBLT::deserialize(const std::vector<uint8_t>& data) {
    size_t expected = m_ * 12;
    if (data.size() < expected) {
        fprintf(stderr, "IBLT::deserialize: expected %zu bytes, got %zu\n",
                expected, data.size());
        return;
    }
    for (int i = 0; i < m_; ++i) {
        const uint8_t* p = data.data() + i * 12;
        uint64_t sum;
        int32_t  cnt;
        memcpy(&sum, p,     8);
        memcpy(&cnt, p + 8, 4);
        cells_[i].sum   = sum;
        cells_[i].count = cnt;
    }
}

// ---- Sparse delta ----

std::vector<uint8_t> IBLT::deltaSerialize() const {
    // Count non-zero cells for sparse encoding
    int nnz = 0;
    for (int i = 0; i < m_; ++i)
        if (cells_[i].count != 0)
            ++nnz;

    // Format: [4B nnz] [for each: 4B row, 4B col, 8B sum, 4B count]
    std::vector<uint8_t> buf(4 + nnz * 20);
    memcpy(buf.data(), &nnz, 4);

    int off = 4;
    for (int i = 0; i < k_; ++i) {
        for (int j = 0; j < ell_; ++j) {
            int idx = i * ell_ + j;
            if (cells_[idx].count != 0) {
                uint8_t* p = buf.data() + off;
                int ro = i, co = j;
                uint64_t s = cells_[idx].sum;
                int32_t  c = cells_[idx].count;
                memcpy(p,      &ro, 4);
                memcpy(p + 4,  &co, 4);
                memcpy(p + 8,  &s,  8);
                memcpy(p + 16, &c,  4);
                off += 20;
            }
        }
    }
    return buf;
}

void IBLT::deltaDeserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 4) return;
    int nnz = 0;
    memcpy(&nnz, data.data(), 4);

    const uint8_t* p = data.data() + 4;
    for (int t = 0; t < nnz; ++t) {
        int ro, co;
        uint64_t s;
        int32_t  c;
        memcpy(&ro, p,      4);
        memcpy(&co, p + 4,  4);
        memcpy(&s,  p + 8,  8);
        memcpy(&c,  p + 16, 4);
        p += 20;

        int idx = ro * ell_ + co;
        if (M_ != 0) cells_[idx].sum = (cells_[idx].sum + s) % M_;
        else         cells_[idx].sum += s;
        cells_[idx].count += c;
    }
}

// ---- Parse delta cell positions (without applying) ----

std::set<std::pair<int,int>> IBLT::deltaCells(
    const std::vector<uint8_t>& data) {
    std::set<std::pair<int,int>> result;
    if (data.size() < 4) return result;
    int nnz = 0;
    memcpy(&nnz, data.data(), 4);
    const uint8_t* p = data.data() + 4;
    for (int t = 0; t < nnz; ++t) {
        int ro, co;
        memcpy(&ro, p,      4);
        memcpy(&co, p + 4,  4);
        p += 20;
        result.insert({ro, co});
    }
    return result;
}

// ---- Queries ----

bool IBLT::isEmpty() const {
    for (int i = 0; i < m_; ++i)
        if (cells_[i].count != 0)
            return false;
    return true;
}

int IBLT::nonZeroCells() const {
    int nz = 0;
    for (int i = 0; i < m_; ++i)
        if (cells_[i].count != 0)
            ++nz;
    return nz;
}

} // namespace fpsu
