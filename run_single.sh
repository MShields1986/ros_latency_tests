#!/bin/bash
# Usage: ./run_single.sh [rmw_service] [extra compose args...]
# where rmw_service is one of: cyclonedds,
#                              fastdds,
#                              fastdds_dynamic,
#                              zenoh,
#                              (iceoryx, iceoryx2, agnocast)
# Defaults to cyclonedds. Runs a single RMW scenario end-to-end and exits;
# plotting is handled separately by ./run_all.sh.
set -euo pipefail

RMW_SERVICE="${1:-cyclonedds}"
shift || true

# Ensure the host-side data dir exists so the mount works cleanly.
mkdir -p data/results

COMPOSE=(docker compose -f docker/docker-compose.yaml)

# Exploratory profiles need --profile exploratory so they become visible.
PROFILE_ARGS=()
case "${RMW_SERVICE}" in
    iceoryx|iceoryx2|agnocast)
        PROFILE_ARGS=(--profile exploratory)
        ;;
esac

"${COMPOSE[@]}" "${PROFILE_ARGS[@]}" up \
    --remove-orphans --build --abort-on-container-exit "${RMW_SERVICE}" "$@"
