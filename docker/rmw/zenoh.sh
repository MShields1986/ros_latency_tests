#!/usr/bin/env bash
# Zenoh RMW implementation (rmw_zenoh_cpp).
set -euo pipefail

apt-get update
# On newer distros this package ships in the ROS apt repo.
if apt-cache show "ros-${ROS_DISTRO}-rmw-zenoh-cpp" >/dev/null 2>&1; then
    apt-get install -y "ros-${ROS_DISTRO}-rmw-zenoh-cpp"
else
    echo "[zenoh.sh] rmw_zenoh_cpp not available for ${ROS_DISTRO} via apt." >&2
    echo "[zenoh.sh] Falling back to building from source is out of scope for this image." >&2
    exit 1
fi

cat > /etc/profile.d/latency_rmw.sh <<'EOF'
export RMW_IMPLEMENTATION=rmw_zenoh_cpp
export ZENOH_ROUTER_CHECK_ATTEMPTS=-1
EOF
chmod +x /etc/profile.d/latency_rmw.sh

# Zenoh needs a router (rmw_zenohd) running in the container before any
# rmw_zenoh_cpp node can discover peers. Install an entrypoint wrapper that
# starts the router, waits for its "Started Zenoh router" log line, then
# execs the requested command. Docker Compose's `command:` override (used by
# run_all.sh) does not replace `entrypoint:`, so this runs on every invocation.
cat > /usr/local/bin/latency_zenoh_entrypoint.sh <<'EOF'
#!/bin/bash
set -e

source "/opt/ros/${ROS_DISTRO}/setup.bash" --
source /ros2_ws/install/setup.bash
source /etc/profile.d/latency_rmw.sh

ZENOHD_LOG=/tmp/zenohd.log
: > "${ZENOHD_LOG}"
ros2 run rmw_zenoh_cpp rmw_zenohd >"${ZENOHD_LOG}" 2>&1 &
ZENOHD_PID=$!

for _ in $(seq 1 200); do
    if ! kill -0 "${ZENOHD_PID}" 2>/dev/null; then
        echo "[latency_zenoh_entrypoint] rmw_zenohd exited early; log:" >&2
        cat "${ZENOHD_LOG}" >&2
        exit 1
    fi
    if grep -q "Started Zenoh router" "${ZENOHD_LOG}" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

if ! grep -q "Started Zenoh router" "${ZENOHD_LOG}" 2>/dev/null; then
    echo "[latency_zenoh_entrypoint] rmw_zenohd did not report ready within 20s; log:" >&2
    cat "${ZENOHD_LOG}" >&2
    exit 1
fi

exec "$@"
EOF
chmod +x /usr/local/bin/latency_zenoh_entrypoint.sh
