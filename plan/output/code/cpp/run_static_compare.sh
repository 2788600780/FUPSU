#!/bin/bash
# F-PSU Static Baseline Comparison — Asiacrypt 2026
# Apple-to-apple: delta (F-PSU) vs static (full IBLT every epoch)
set -e
cd "$(dirname "$0")/build"
BIN=./fpsu_net
EPOCHS=5

run_one() {
    local n=$1 delta=$2 lambda=$3 static=$4
    local port=$((9600 + RANDOM % 1000))
    local tmp=$(mktemp /tmp/fpsu_srv.XXXXXX)

    $BIN -s -p $port -n $n -d $delta -l $lambda -e $EPOCHS $static > $tmp 2>&1 &
    local spid=$!
    sleep 0.3
    $BIN -c 127.0.0.1 -p $port -n $n -d $delta -l $lambda -e $EPOCHS $static > /tmp/fpsu_cli.txt 2>&1 || true
    wait $spid 2>/dev/null || true

    cat $tmp
    rm -f $tmp /tmp/fpsu_cli.txt
}

extract_epoch_data() {
    # Input: server output text. Output: epoch0_total epoch_gt0_avg xfer_bytes_gt0
    local txt="$1"
    local ep0_total="" ep_gt0_sum=0 ep_gt0_count=0 xfer_bytes=0 xfer_count=0

    while IFS= read -r line; do
        if [[ "$line" =~ Epoch\ ([0-9]+)\ .*total=([0-9.]+)ms.*bytes=([0-9]+) ]]; then
            local ep="${BASH_REMATCH[1]}"
            local total="${BASH_REMATCH[2]}"
            local bytes="${BASH_REMATCH[3]}"
            if [ "$ep" = "0" ]; then
                ep0_total="$total"
            else
                ep_gt0_sum=$(echo "$ep_gt0_sum + $total" | bc)
                ep_gt0_count=$((ep_gt0_count + 1))
                xfer_bytes=$((xfer_bytes + bytes))
                xfer_count=$((xfer_count + 1))
            fi
        fi
    done <<< "$txt"

    local ep_gt0_avg="0"
    local xfer_avg="0"
    if [ "$ep_gt0_count" -gt 0 ]; then
        ep_gt0_avg=$(echo "scale=2; $ep_gt0_sum / $ep_gt0_count" | bc)
        xfer_avg=$((xfer_bytes / xfer_count))
    fi
    echo "$ep0_total $ep_gt0_avg $xfer_avg"
}

printf "%-10s %-8s | %-10s %-10s %-10s | %-10s %-10s %-10s | %-10s\n" \
    "n" "|Δ|" "E0_delta" "E>0_delta" "Δ_KB" "E0_static" "E>0_static" "Full_KB" "Speedup"
printf "%s\n" "----------|--------|-----------|-----------|-----------|-----------|-----------|-----------|----------"

for params in \
    "1024 256 3.0" \
    "4096 256 3.0" \
    "16384 1024 3.0" \
    "65536 4096 2.5" \
    "262144 16384 2.0"
do
    read n delta lambda <<< "$params"

    d_txt=$(run_one $n $delta $lambda "")
    read d_e0 d_egt0 d_xfer <<< $(extract_epoch_data "$d_txt")

    s_txt=$(run_one $n $delta $lambda "-S")
    read s_e0 s_egt0 s_xfer <<< $(extract_epoch_data "$s_txt")

    d_kb=$(echo "scale=1; $d_xfer / 1000" | bc)
    s_kb=$(echo "scale=1; $s_xfer / 1000" | bc)
    spd=$(echo "scale=1; $s_egt0 / $d_egt0" | bc)

    printf "%-10s %-8s | %-10s %-10s %-10s | %-10s %-10s %-10s | %s×\n" \
        "$n" "$delta" "$d_e0" "$d_egt0" "$d_kb" "$s_e0" "$s_egt0" "$s_kb" "$spd"

    echo ""
done

echo ""
echo "Delta mode: sparse delta IBLT (O(k|Δ|) per epoch)"
echo "Static mode: full IBLT re-encode every epoch (O(n) per epoch)"
echo "Same hardware, same codebase, same IBLT implementation."
