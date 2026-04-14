#!/bin/bash
# Usage:
#   ./run_all.sh                            # default matrix
#   ./run_all.sh "tcp udp"                  # legacy: override transports only
#   MATRIX_TRANSPORTS="zerocopy tcp udp" \
#   MATRIX_MESSAGES="sensor_msgs/Image sensor_msgs/PointCloud2" \
#   MATRIX_PAYLOADS="1024 1048576" \
#   MATRIX_NODES="2 5" \
#   MATRIX_THREADS="1 4" \
#   ./run_all.sh
#
# Runs the cartesian product (transport × message × payload × num_nodes ×
# num_threads) and then invokes the plot container to produce violin plots.
set -euo pipefail

MATRIX_TRANSPORTS="${MATRIX_TRANSPORTS:-zerocopy tcp tcp_nodelay udp}"
MATRIX_MESSAGES="${MATRIX_MESSAGES:-std_msgs/Float64 geometry_msgs/TransformStamped sensor_msgs/PointCloud2}"
MATRIX_PAYLOADS="${MATRIX_PAYLOADS:-1048576}"
MATRIX_NODES="${MATRIX_NODES:-2}"
MATRIX_THREADS="${MATRIX_THREADS:-2}"
MATRIX_RATE="${MATRIX_RATE:-20.0}"
MATRIX_DURATION="${MATRIX_DURATION:-600.0}"

# Legacy positional override.
if [[ $# -ge 1 && -n "${1:-}" ]]; then
    MATRIX_TRANSPORTS="$1"
fi

COMPOSE=(docker compose -f docker/docker-compose.yaml)
mkdir -p data/results

total=0
for _ in $MATRIX_TRANSPORTS; do
    for _ in $MATRIX_MESSAGES; do
        for _ in $MATRIX_PAYLOADS; do
            for _ in $MATRIX_NODES; do
                for _ in $MATRIX_THREADS; do
                    total=$((total + 1))
                done
            done
        done
    done
done

echo "[run_all] matrix: $total combinations"
echo "           transports=[$MATRIX_TRANSPORTS]"
echo "           messages=[$MATRIX_MESSAGES]"
echo "           payloads=[$MATRIX_PAYLOADS]"
echo "           nodes=[$MATRIX_NODES]"
echo "           threads=[$MATRIX_THREADS]"
echo "           rate=$MATRIX_RATE  duration=$MATRIX_DURATION"

i=0
for tp in $MATRIX_TRANSPORTS; do
    for msg in $MATRIX_MESSAGES; do
        for payload in $MATRIX_PAYLOADS; do
            for nodes in $MATRIX_NODES; do
                for threads in $MATRIX_THREADS; do
                    i=$((i + 1))
                    echo
                    echo "=========================================================="
                    echo "[run_all] ($i/$total) tp=$tp msg=$msg payload=${payload}B" \
                         "nodes=$nodes threads=$threads"
                    echo "=========================================================="
                    "${COMPOSE[@]}" run --rm --build latency \
                        python3 /catkin_ws/src/latency_tests/launch/latency_pipeline.py \
                            transport:="$tp" \
                            message_type:="$msg" \
                            payload_bytes:="$payload" \
                            num_nodes:="$nodes" \
                            num_threads:="$threads" \
                            publish_rate_hz:="$MATRIX_RATE" \
                            duration_s:="$MATRIX_DURATION"
                done
            done
        done
    done
done

echo
echo "=========================================================="
echo "[run_all] generating violin plots from data/results..."
echo "=========================================================="
"${COMPOSE[@]}" run --rm --build plot \
    --title "All runs ($(date '+%Y-%m-%d %H:%M:%S'))"
