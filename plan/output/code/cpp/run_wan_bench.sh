#!/bin/bash
# F-PSU WAN Network Benchmark
# Simulates 4 network profiles via in-app delay simulation.
# Asiacrypt 2026.

BIN="/Users/lc/agent/fupsu/计划/output/code/cpp/build/fpsu_net"
PORT=9876
OUT="/tmp/fpsu_wan_results.txt"

echo "================================================================"
echo "  F-PSU WAN Network Benchmark"
echo "  Date: $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================================"

# Network profiles: name, RTT(ms), BW(Mbps)
profiles=(
    "LAN         0     0"
    "Fast_WAN    40    200"
    "Medium_WAN  80    50"
    "Slow_WAN    80    5"
)

# Test params: n, delta, lambda, epochs
tests=(
    "16384    1024   3.0  5"
    "65536    4096   2.5  5"
    "262144   16384  2.0  3"
    "1048576  65536  2.0  3"
)

for prof in "${profiles[@]}"; do
    read pname rtt bw <<< "$prof"
    echo ""
    echo "---- Profile: $pname (RTT=${rtt}ms, BW=${bw}Mbps) ----"
    echo "  n         |Δ|     Epoch  |U|       Bytes      Total(ms)  Xfer(ms)"
    echo "  ---------- -------- -----  -------- ---------  ---------  -------"

    for t in "${tests[@]}"; do
        read n d lambda epochs <<< "$t"

        # Skip 1M for slow WAN (would take too long)
        if [ "$pname" = "Slow_WAN" ] && [ "$n" -ge 262144 ]; then
            # Still run but with fewer epochs
            if [ "$n" -ge 1048576 ]; then
                echo "  (skipping n=$n for Slow WAN — would take >60s/epoch)"
                continue
            fi
        fi

        "$BIN" -s -p "$PORT" -n "$n" -d "$d" -l "$lambda" -e "$epochs" \
            -r "$rtt" -b "$bw" > /tmp/fpsu_srv.out 2>&1 &
        SRV=$!
        sleep 1

        "$BIN" -c 127.0.0.1 -p "$PORT" -n "$n" -d "$d" -l "$lambda" -e "$epochs" \
            -r "$rtt" -b "$bw" > /tmp/fpsu_cli.out 2>&1

        wait $SRV 2>/dev/null

        cli_avg_total=$(grep "Avg total:" /tmp/fpsu_cli.out | awk '{print $(NF-1)}' | sed 's/ms//')
        cli_avg_xfer=$(grep "Avg xfer:" /tmp/fpsu_cli.out | awk '{print $NF}' | sed 's/ms//')
        full_bytes=$(grep "IBLT full:" /tmp/fpsu_srv.out | awk '{print $NF}')

        printf "  %-10s %-7s  " "$n" "$d"
        grep "Epoch" /tmp/fpsu_cli.out | while read line; do
            ep=$(echo "$line" | sed 's/.*Epoch \([0-9]*\).*/\1/')
            bu=$(echo "$line" | sed 's/.*bytes=\([0-9]*\).*/\1/')
            tot_ms=$(echo "$line" | sed 's/.*total=\([0-9.]*\)ms.*/\1/')
            xfer_ms=$(echo "$line" | sed 's/.*xfer=\([0-9.]*\)ms.*/\1/')
            union=$(echo "$line" | sed 's/.*|U|=\([0-9]*\).*/\1/')
            printf "%-5s %-8s %-10s %-9s %-8s\n" "$ep" "$union" "$bu" "$tot_ms" "$xfer_ms"
        done
        printf "  %-10s %-7s  AVG: total=%sms xfer=%sms\n" "" "" "$cli_avg_total" "$cli_avg_xfer"
    done
done

rm -f /tmp/fpsu_srv.out /tmp/fpsu_cli.out
echo ""
echo "================================================================"
echo "  Done."
echo "================================================================"
