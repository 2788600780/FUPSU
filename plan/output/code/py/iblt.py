"""
F-PSU IBLT Implementation — Asiacrypt 2026
Invertible Bloom Lookup Table with delta encoding and incremental peeling.

Reference: Piske & Trieu, "Is PSI Really Faster Than PSU?", Eurocrypt 2026.
"""

import hashlib
import random
import struct
import time
from typing import List, Set, Tuple, Optional


class IBLT:
    """Invertible Bloom Lookup Table (k subtables, each with ell cells).

    Each cell stores:
      - sum: mod-M sum of element identifiers
      - cnt: count of elements hashed to this cell

    Encoding is linear: Encode(A ∪ B) = Encode(A) ⊕ Encode(B) for disjoint A,B.
    """

    def __init__(self, k: int, ell: int, M: int = 2**64):
        self.k = k
        self.ell = ell
        self.m = k * ell
        self.M = M
        self.sum = [[0 for _ in range(ell)] for _ in range(k)]
        self.cnt = [[0 for _ in range(ell)] for _ in range(k)]

    # ---- Hashing ----

    def _hash(self, x: bytes, i: int) -> int:
        """Hash element x to position in subtable i (SHA-256 based)."""
        h = hashlib.sha256(struct.pack('>I', i) + x).digest()
        return int.from_bytes(h[:8], 'big') % self.ell

    def _hash_positions(self, x: bytes) -> List[Tuple[int, int]]:
        return [(i, self._hash(x, i)) for i in range(self.k)]

    # ---- Basic Operations ----

    def insert(self, x: bytes):
        x_int = int.from_bytes(x, 'big') % self.M
        for i, j in self._hash_positions(x):
            self.sum[i][j] = (self.sum[i][j] + x_int) % self.M
            self.cnt[i][j] += 1

    def delete(self, x: bytes):
        x_int = int.from_bytes(x, 'big') % self.M
        for i, j in self._hash_positions(x):
            self.sum[i][j] = (self.sum[i][j] - x_int) % self.M
            self.cnt[i][j] -= 1

    def encode(self, X: Set[bytes]):
        for x in X:
            self.insert(x)

    @classmethod
    def from_set(cls, X: Set[bytes], k: int, n_max: int, M: int = 2**64,
                 expansion: float = 3.0) -> 'IBLT':
        ell = int(expansion * n_max / k) + 1
        iblt = cls(k, ell, M)
        iblt.encode(X)
        return iblt

    # ---- Peeling ----

    def peelable_cells(self) -> List[Tuple[int, int, int]]:
        result = []
        for i in range(self.k):
            for j in range(self.ell):
                if self.cnt[i][j] == 1:
                    result.append((i, j, self.sum[i][j]))
        return result

    def list_entries(self) -> Tuple[Set[bytes], bool]:
        """Full peel: returns (recovered_elements, success)."""
        V = set()
        iblt = self
        Q = [(i, j) for i in range(self.k) for j in range(self.ell)]

        while Q:
            V_new = set()
            for i, j in Q:
                if iblt.cnt[i][j] == 1:
                    x_int = iblt.sum[i][j]
                    x_bytes = x_int.to_bytes(8, 'big')
                    V_new.add(x_bytes)

            if not V_new:
                break

            V |= V_new
            for x in V_new:
                iblt.delete(x)

            Q = []
            for x in V_new:
                for i in range(iblt.k):
                    Q.append((i, iblt._hash(x, i)))

        success = all(iblt.cnt[i][j] == 0 for i in range(iblt.k)
                      for j in range(iblt.ell))
        return V, success

    # ---- Delta IBLT (linearity exploit) ----

    def merge_add(self, other: 'IBLT'):
        assert self.k == other.k and self.ell == other.ell
        for i in range(self.k):
            for j in range(self.ell):
                self.sum[i][j] = (self.sum[i][j] + other.sum[i][j]) % self.M
                self.cnt[i][j] += other.cnt[i][j]

    def merge_sub(self, other: 'IBLT'):
        assert self.k == other.k and self.ell == other.ell
        for i in range(self.k):
            for j in range(self.ell):
                self.sum[i][j] = (self.sum[i][j] - other.sum[i][j]) % self.M
                self.cnt[i][j] -= other.cnt[i][j]

    def modified_cells(self) -> Set[Tuple[int, int]]:
        """Return positions of all cells with non-zero count."""
        result = set()
        for i in range(self.k):
            for j in range(self.ell):
                if self.cnt[i][j] != 0:
                    result.add((i, j))
        return result

    # ---- Incremental Peeling ----

    def incremental_peel(self, start_cells: Set[Tuple[int, int]]) \
            -> Tuple[Set[bytes], int, bool]:
        """Peel starting from specific cells (delta-modified).

        Returns: (peeled_elements, cascade_size, success)
        """
        V = set()
        iblt = self
        Q = list(start_cells)
        cascade = len(Q)

        while Q:
            V_new = set()
            for i, j in Q:
                if iblt.cnt[i][j] == 1:
                    x_int = iblt.sum[i][j]
                    x_bytes = x_int.to_bytes(8, 'big')
                    V_new.add(x_bytes)

            if not V_new:
                break

            V |= V_new
            for x in V_new:
                iblt.delete(x)

            Q = []
            for x in V_new:
                for i in range(iblt.k):
                    Q.append((i, iblt._hash(x, i)))
                    cascade += 1

        success = all(iblt.cnt[i][j] == 0 for i in range(iblt.k)
                      for j in range(iblt.ell))
        return V, cascade, success

    # ---- Metrics ----

    def serialize_size(self) -> int:
        return self.k * self.ell * (64 + 32)

    def non_zero_cells(self) -> int:
        return sum(1 for i in range(self.k) for j in range(self.ell)
                   if self.cnt[i][j] != 0)

    def delta_size_bits(self, positions: Set[Tuple[int, int]]) -> int:
        return len(positions) * (64 + 32 + 32)

    # ---- Copy helper ----

    def copy_empty(self) -> 'IBLT':
        return IBLT(self.k, self.ell, self.M)


def random_set(n: int, seed: int = 0) -> Set[bytes]:
    """Generate n random 8-byte elements with a given seed."""
    rng = random.Random(seed)
    s = set()
    while len(s) < n:
        s.add(rng.randbytes(8))
    return s


def correctness_tests():
    """Quick correctness checks for IBLT operations."""
    # Test 1: Basic encode + peel
    for n in [10, 100, 1000]:
        X = random_set(n, 42)
        ib = IBLT.from_set(X, k=4, n_max=n, expansion=3.0)
        V, ok = ib.list_entries()
        assert ok and V == X, f"Basic test failed for n={n}"

    # Test 2: Delta IBLT = sum of parts
    X = random_set(500, 42)
    Y = random_set(300, 99)
    ell = int(3.0 * 800 / 4) + 1
    ib_x = IBLT(4, ell); ib_x.encode(X)
    ib_y = IBLT(4, ell); ib_y.encode(Y)
    ib_merged = IBLT(4, ell)
    ib_merged.merge_add(ib_x)
    ib_merged.merge_add(ib_y)
    V_merged, ok = ib_merged.list_entries()
    assert ok, "Merge test failed"

    ib_direct = IBLT(4, ell); ib_direct.encode(X | Y)
    V_direct, _ = ib_direct.list_entries()
    assert V_merged == V_direct, "Merge vs direct mismatch"

    # Test 3: Incremental peel correctness
    X = random_set(200, 42)
    ib = IBLT(4, ell); ib.encode(X)
    modified = set()
    for x in X:
        for i in range(4):
            modified.add((i, ib._hash(x, i)))
    ib2 = IBLT(4, ell); ib2.encode(X)
    V_incr, cascade, ok = ib2.incremental_peel(modified)
    assert ok and V_incr == X, "Incremental peel failed"

    print(f"  All correctness tests passed (n=10,100,1000; merge; incremental peel)")
