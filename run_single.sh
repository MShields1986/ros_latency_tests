#!/bin/bash
# Usage: ./run_single.sh [transport] [extra args...]
# transport is one of: zerocopy, tcp, tcp_nodelay, udp (default: tcp).
# Extra args are forwarded to the pipeline launcher as key:=value.
set -euo pipefail

TRANSPORT="${1:-tcp}"
shift || true

mkdir -p data/results

COMPOSE=(docker compose -f docker/docker-compose.yaml)

"${COMPOSE[@]}" run --rm --build latency \
    python3 /catkin_ws/src/latency_tests/launch/latency_pipeline.py \
        transport:="${TRANSPORT}" "$@"
