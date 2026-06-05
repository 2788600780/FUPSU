#!/bin/bash
# F-PSU Localhost Network Benchmark (with delta exchange)
# Asiacrypt 2026.
set -e

BIN="/Users/lc/agent/fupsu/计划/output/code/cpp/build/fpsu_net"
PORT=9876

echo "================================================================"
echo "  F-PSU End-to-End Network Benchmark (localhost, delta exchange)"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================================"
echo "  n         |Δ|     λ   Epoch  Bytes     Total(ms)  Xfer(ms)  Peel(ms)"
echo "  --------- -------- ---  -----  --------  ---------  --------  -------"

tests=(
    "1024     256"
    "4096     256"
    "16384    1024"
    "65536    4096"
    "262144   16384"
    "1048576  65536"
)

for t in "${tests[@]}"; do
    read n d <<< "$t"
    lambda=2.0
    epochs=5

    # Small runs use more epochs, large runs fewer
    if [ "$n" -ge 262144 ]; then
        epochs=3
    fi

    "$BIN" -s -p "$PORT" -n "$n" -d "$d" -l "$lambda" -e "$epochs" \
        > /tmp/fpsu_srv.out 2>&1 &
    SRV=$!
    sleep 1

    "$BIN" -c 127.0.0.1 -p "$PORT" -n "$n" -d "$d" -l "$lambda" -e "$epochs" \
        > /tmp/fpsu_cli.out 2>&1

    wait $SRV 2>/dev/null

    # Extract summary stats
    cli_avg_total=$(grep "Avg total:" /tmp/fpsu_cli.out | awk '{print $(NF-1)}' | sed 's/ms//')
    cli_avg_xfer=$(grep "Avg xfer:" /tmp/fpsu_cli.out | awk '{print $NF}' | sed 's/ms//')
    full_bytes=$(grep "IBLT full:" /tmp/fpsu_srv.out | awk '{print $NF}')

    printf "\n%-10s %-7s %-5s\n" "$n" "$d" "$lambda"
    printf "  Full IBLT: %s bytes\n" "$full_bytes"

    # Per-epoch detail
    grep "Epoch" /tmp/fpsu_cli.out | while read line; do
        echo "  $line"
    done

    printf "  Avg: total=%sms xfer=%sms\n" "$cli_avg_total" "$cli_avg_xfer"
done

echo ""
echo "================================================================"
echo "  Done."
echo "================================================================"
rm -f /tmp/fpsu_srv.out /tmp/fpsu_cli.out
