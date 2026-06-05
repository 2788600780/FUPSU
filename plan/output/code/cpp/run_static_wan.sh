#!/bin/bash
# F-PSU WAN Static Baseline Comparison — Asiacrypt 2026
# Delta vs Static across 4 WAN profiles
set -e
cd "$(dirname "$0")/build"
BIN=./fpsu_net

run_wan() {
    local n=$1 delta=$2 lambda=$3 rtt=$4 bw=$5 static=$6
    local port=$((9700 + RANDOM % 1000))
    local tmp=$(mktemp /tmp/fpsu_wan.XXXXXX)

    $BIN -s -p $port -n $n -d $delta -l $lambda -e 4 -r $rtt -b $bw $static > $tmp 2>&1 &
    local spid=$!
    sleep 0.5
    $BIN -c 127.0.0.1 -p $port -n $n -d $delta -l $lambda -e 4 -r $rtt -b $bw $static > /dev/null 2>&1 || true
    wait $spid 2>/dev/null || true
    cat $tmp
    rm -f $tmp
}

extract_epoch_data() {
    local ep0_total="" ep_gt0_sum=0 ep_gt0_count=0 xfer_bytes=0 xfer_count=0
    while IFS= read -r line; do
        if [[ "$line" =~ Epoch\ ([0-9]+)\ .*total=([0-9.]+)ms.*bytes=([0-9]+) ]]; then
            local ep="${BASH_REMATCH[1]}" total="${BASH_REMATCH[2]}" bytes="${BASH_REMATCH[3]}"
            if [ "$ep" = "0" ]; then ep0_total="$total"
            else
                ep_gt0_sum=$(echo "$ep_gt0_sum + $total" | bc)
                ep_gt0_count=$((ep_gt0_count + 1))
                xfer_bytes=$((xfer_bytes + bytes)); xfer_count=$((xfer_count + 1))
            fi
        fi
    done <<< "$1"
    local ep_gt0_avg="0" xfer_avg="0"
    if [ "$ep_gt0_count" -gt 0 ]; then
        ep_gt0_avg=$(echo "scale=2; $ep_gt0_sum / $ep_gt0_count" | bc)
        xfer_avg=$((xfer_bytes / xfer_count))
    fi
    echo "$ep_gt0_avg $xfer_avg"
}

echo "F-PSU Static Baseline — WAN Comparison"
echo "======================================="
echo ""
printf "%-14s %-8s | %-12s %-12s %-10s | %-12s %-12s %-10s | %-8s\n" \
    "Network" "n" "Delta(ms)" "Delta(KB)" "Speedup" "Static(ms)" "Static(KB)" "Slowdown" "Comm.↓"
printf "%s\n" "--------------|--------|-------------|-------------|----------|-------------|-------------|----------|--------"

for profile in \
    "LAN     0    0" \
    "FastWAN 40   200" \
    "MedWAN  80   50" \
    "SlowWAN 80   5"
do
    read label rtt bw <<< "$profile"
    for n_delta in "16384 1024 3.0" "65536 4096 2.5"; do
        read n delta lambda <<< "$n_delta"

        d_txt=$(run_wan $n $delta $lambda $rtt $bw "")
        read d_egt0 d_xfer <<< $(extract_epoch_data "$d_txt")

        s_txt=$(run_wan $n $delta $lambda $rtt $bw "-S")
        read s_egt0 s_xfer <<< $(extract_epoch_data "$s_txt")

        spd=$(echo "scale=1; $s_egt0 / $d_egt0" | bc)
        d_kb=$(echo "scale=1; $d_xfer / 1000" | bc)
        s_kb=$(echo "scale=1; $s_xfer / 1000" | bc)
        ratio=$(echo "scale=1; $s_xfer / $d_xfer" | bc)

        printf "%-14s %-8s | %-12s %-12s %-10s | %-12s %-12s %-10s | %s×\n" \
            "$label" "$n" "${d_egt0}ms" "${d_kb}KB" "${spd}×" "${s_egt0}ms" "${s_kb}KB" "-" "$ratio"
    done
    echo ""
done

echo ""
echo "Delta = sparse delta IBLT (F-PSU). Static = full IBLT every epoch."
echo "Same hardware, same codebase, same IBLT implementation."
