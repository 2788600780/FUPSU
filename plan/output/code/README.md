# F-PSU: Forward-Private Updatable Private Set Union

Reference implementation for the USENIX submission. (Repository will be made public upon publication.)

## Directory Structure

```
code/
├── py/                  # Python IBLT prototype
│   ├── iblt.py          # IBLT implementation (encode, decode, delta, peeling)
│   ├── benchmark.py     # Python benchmark suite
│   └── bench_results.json  # Benchmark results
├── cpp/                 # C++ primitive benchmarks (libOTe)
│   ├── benchmark.cpp    # OT, SoftSpokenOT, KOS, IBLT benchmarks
│   └── CMakeLists.txt
└── libOTe/              # libOTe library (git submodule)
```

## Python Prototype

Validates correctness of:

- IBLT encoding, listing, and peeling
- Delta IBLT encoding (incremental updates)
- Incremental peeling (maintained IBLT + delta)
- Cascade bound measurements
- Communication reduction analysis

```bash
cd py
python3 benchmark.py
```

## C++ Benchmark Suite

Measures wall-clock performance of cryptographic primitives used in F-PSU.

### Build

Requires: CMake ≥ 3.20, Apple Clang (or GCC ≥ 12), OpenSSL, libsodium.

```bash
# First-time setup: build libOTe
cd ../libOTe
python3 build.py --setup --boost --sodium
cd out/build/osx && cmake --build . --target frontend_libOTe -j

# Then build our benchmark
cd ../../../../cpp
mkdir -p build && cd build
cmake .. && make -j
DYLD_LIBRARY_PATH="../../libOTe/out/install/osx/lib" ./benchmark
```

### Results (Apple M-series, macOS, single-threaded, localhost)

| Primitive | n | Time (ms) | Throughput |
|---|---|---|---|
| Base OT (SimplestOT) | 128 | 9.70 | — |
| KOS OT Extension | 10⁴ | 1.96 | 5.1×10⁶/s |
| KOS OT Extension | 10⁵ | 19.9 | 5.0×10⁶/s |
| KOS OT Extension | 10⁶ | 200 | 5.0×10⁶/s |
| SoftSpokenOT (f=8) | 10⁴ | 0.84 | 11.9×10⁶/s |
| SoftSpokenOT (f=8) | 10⁵ | 7.68 | 13.0×10⁶/s |
| IBLT Encode (k=4, e=3.0) | 10⁵ | 0.09 | 1.1×10⁹/s |
| IBLT Encode (k=4, e=3.0) | 10⁶ | 5.15 | 1.94×10⁸/s |

## Paper

The compiled paper is at `../usenix/main.pdf`.

## License

MIT
