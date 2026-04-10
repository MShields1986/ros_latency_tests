#!/bin/bash
# Usage:
#   ./run_all.sh                           # default matrix
#   ./run_all.sh "cyclonedds zenoh"        # legacy: override just the RMW list
#   MATRIX_MESSAGES="sensor_msgs/Image sensor_msgs/PointCloud2" \
#   MATRIX_PAYLOADS="1024 1048576" \
#   MATRIX_NODES="2 5" \
#   MATRIX_THREADS="1 4" \
#   MATRIX_IPCS="false true" \
#   ./run_all.sh
#
# Runs the full cartesian product (RMW × message × payload × num_nodes ×
# num_threads × intraproc) and then invokes the plot container to produce
# violin plots from every resulting CSV in data/results.
#
# Any axis can be overridden via environment variable. Space-separated.
# Default list for RMW covers the four Tier 1 middlewares that "just work"
# on Kilted; exploratory services (iceoryx, iceoryx2, agnocast) are opt-in
# via explicit MATRIX_RMWS.
set -euo pipefail

# -----------------------------------------------------------------------------
# Matrix definition. Every axis is a space-separated list — edit inline or
# override from the environment. The cartesian product is executed in order.
# -----------------------------------------------------------------------------
MATRIX_RMWS="${MATRIX_RMWS:-cyclonedds fastdds fastdds_dynamic zenoh}"
MATRIX_MESSAGES="${MATRIX_MESSAGES:-std_msgs/Float64 geometry_msgs/TransformStamped sensor_msgs/PointCloud2}"
MATRIX_PAYLOADS="${MATRIX_PAYLOADS:-1048576}"
MATRIX_NODES="${MATRIX_NODES:-2}"
MATRIX_THREADS="${MATRIX_THREADS:-2}"
MATRIX_IPCS="${MATRIX_IPCS:-false true}"
MATRIX_RATE="${MATRIX_RATE:-20.0}"
MATRIX_DURATION="${MATRIX_DURATION:-600.0}"

# Legacy positional override: `./run_all.sh "cyclonedds zenoh"` still works
# and replaces MATRIX_RMWS so existing workflows don't break.
if [[ $# -ge 1 && -n "${1:-}" ]]; then
    MATRIX_RMWS="$1"
fi

COMPOSE=(docker compose -f docker/docker-compose.yaml)

mkdir -p data/results

# Exploratory services live behind a compose profile.
is_exploratory() {
    case "$1" in
        iceoryx|iceoryx2|agnocast) return 0 ;;
        *) return 1 ;;
    esac
}

total=0
for _ in $MATRIX_RMWS; do
    for _ in $MATRIX_MESSAGES; do
        for _ in $MATRIX_PAYLOADS; do
            for _ in $MATRIX_NODES; do
                for _ in $MATRIX_THREADS; do
                    for _ in $MATRIX_IPCS; do
                        total=$((total + 1))
                    done
                done
            done
        done
    done
done

echo "[run_all] matrix: $total combinations"
echo "           rmws=[$MATRIX_RMWS]"
echo "           messages=[$MATRIX_MESSAGES]"
echo "           payloads=[$MATRIX_PAYLOADS]"
echo "           nodes=[$MATRIX_NODES]"
echo "           threads=[$MATRIX_THREADS]"
echo "           ipcs=[$MATRIX_IPCS]"
echo "           rate=$MATRIX_RATE  duration=$MATRIX_DURATION"

i=0
for svc in $MATRIX_RMWS; do
    PROFILE_ARGS=()
    if is_exploratory "$svc"; then
        PROFILE_ARGS=(--profile exploratory)
    fi

    for msg in $MATRIX_MESSAGES; do
        for payload in $MATRIX_PAYLOADS; do
            for nodes in $MATRIX_NODES; do
                for threads in $MATRIX_THREADS; do
                    for ipc in $MATRIX_IPCS; do
                        i=$((i + 1))
                        echo
                        echo "=========================================================="
                        echo "[run_all] ($i/$total) svc=$svc msg=$msg payload=${payload}B" \
                             "nodes=$nodes threads=$threads ipc=$ipc"
                        echo "=========================================================="
                        "${COMPOSE[@]}" "${PROFILE_ARGS[@]}" run --rm --build "$svc" \
                            ros2 launch latency_tests latency_pipeline.launch.py \
                                message_type:="$msg" \
                                payload_bytes:="$payload" \
                                num_nodes:="$nodes" \
                                num_threads:="$threads" \
                                use_intra_process_comms:="$ipc" \
                                publish_rate_hz:="$MATRIX_RATE" \
                                duration_s:="$MATRIX_DURATION"
                    done
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
