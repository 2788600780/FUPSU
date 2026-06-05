#!/bin/bash
# F-PSU Localhost Network Benchmark
# Measures end-to-end epoch time over TCP on localhost.
# Asiacrypt 2026.

BUILD_DIR="/Users/lc/agent/fupsu/计划/output/code/cpp/build"
BIN="$BUILD_DIR/fpsu_net"
PORT=9876

echo "================================================================"
echo "  F-PSU End-to-End Network Benchmark (localhost)"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================================"
echo ""
echo "  n        |Δ|     Epochs  IBLT(bytes)  AvgTotal(ms)  AvgXfer(ms)"
echo "  -------- -------- ------  -----------  ------------  -----------"

tests=(
    "1024    256    5"
    "4096    256    5"
    "16384   1024   5"
    "65536   4096   3"
    "262144  16384  3"
    "1048576 65536  3"
)

for t in "${tests[@]}"; do
    read n d e <<< "$t"

    "$BIN" -s -p "$PORT" -n "$n" -d "$d" -e "$e" > /tmp/fpsu_server.out 2>&1 &
    SERVER_PID=$!
    sleep 1

    "$BIN" -c 127.0.0.1 -p "$PORT" -n "$n" -d "$d" -e "$e" > /tmp/fpsu_client.out 2>&1

    wait $SERVER_PID 2>/dev/null

    client_total=$(grep "Avg total:" /tmp/fpsu_client.out | awk '{print $NF}' | sed 's/ms//')
    client_xfer=$(grep "Avg xfer:" /tmp/fpsu_client.out | awk '{print $NF}' | sed 's/ms//')
    iblt_bytes=$(grep "IBLT size:" /tmp/fpsu_server.out | awk '{print $NF}')

    printf "  %-8s %-7s %-6s  %-11s  %-12s  %-11s\n" \
        "$n" "$d" "$e" "$iblt_bytes" "$client_total" "$client_xfer"

    grep "Epoch" /tmp/fpsu_server.out | while read line; do
        echo "           $line"
    done
done

echo ""
echo "================================================================"
echo "  Done."
echo "================================================================"

rm -f /tmp/fpsu_server.out /tmp/fpsu_client.out
