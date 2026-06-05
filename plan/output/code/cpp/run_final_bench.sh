#!/bin/bash
# F-PSU Final Network Benchmark (localhost, delta exchange)
# Asiacrypt 2026 — §7 Evaluation
set -e

BIN="/Users/lc/agent/fupsu/计划/output/code/cpp/build/fpsu_net"
PORT=9876
OUT="/tmp/fpsu_results.txt"

echo "================================================================"
echo "  F-PSU End-to-End LAN Benchmark"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================================"

# Adaptive lambda: larger for small n to guarantee correctness
tests=(
    "1024     256    3.0  5"
    "4096     256    3.0  5"
    "16384    1024   3.0  5"
    "65536    4096   2.5  5"
    "262144   16384  2.0  3"
    "1048576  65536  2.0  3"
)

echo ""
echo "  n        |Δ|     λ   Epoch  |U|      Bytes     Total(ms)  Peel(ms)"
echo "  --------- -------- ---  -----  -------  --------  ---------  -------"

for t in "${tests[@]}"; do
    read n d lambda epochs <<< "$t"

    "$BIN" -s -p "$PORT" -n "$n" -d "$d" -l "$lambda" -e "$epochs" \
        > /tmp/fpsu_srv.out 2>&1 &
    SRV=$!
    sleep 1

    "$BIN" -c 127.0.0.1 -p "$PORT" -n "$n" -d "$d" -l "$lambda" -e "$epochs" \
        > /tmp/fpsu_cli.out 2>&1

    wait $SRV 2>/dev/null

    # Per-epoch detail from client
    cli_avg_total=$(grep "Avg total:" /tmp/fpsu_cli.out | awk '{print $(NF-1)}' | sed 's/ms//')
    full_bytes=$(grep "IBLT full:" /tmp/fpsu_srv.out | awk '{print $NF}')

    printf "  %-8s %-7s %-5s\n" "$n" "$d" "$lambda"
    grep "Epoch" /tmp/fpsu_cli.out | while read line; do
        echo "         $line"
    done
    printf "                                       Avg total: %s ms  |  Full IBLT: %s B\n" "$cli_avg_total" "$full_bytes"
    echo ""
done

# Cleanup
rm -f /tmp/fpsu_srv.out /tmp/fpsu_cli.out
echo "================================================================"
echo "  Done."
echo "================================================================"
