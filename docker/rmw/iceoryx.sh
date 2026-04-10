#!/usr/bin/env bash
# Eclipse iceoryx (v1) shared-memory RMW.
#
# Requires a RouDi daemon to be running alongside the ROS process. The
# docker-compose service launches it in-container via the command override.
set -euo pipefail

apt-get update
if ! apt-cache show "ros-${ROS_DISTRO}-rmw-iceoryx-cpp" >/dev/null 2>&1; then
    echo "[iceoryx.sh] rmw_iceoryx_cpp not available for ${ROS_DISTRO} via apt." >&2
    exit 1
fi

apt-get install -y \
    "ros-${ROS_DISTRO}-rmw-iceoryx-cpp" \
    "ros-${ROS_DISTRO}-iceoryx-posh" \
    "ros-${ROS_DISTRO}-iceoryx-binding-c"

cat > /etc/profile.d/latency_rmw.sh <<'EOF'
export RMW_IMPLEMENTATION=rmw_iceoryx_cpp
EOF
chmod +x /etc/profile.d/latency_rmw.sh
