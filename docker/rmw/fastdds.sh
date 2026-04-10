#!/usr/bin/env bash
# Fast DDS (rmw_fastrtps_cpp) — ships with most ROS 2 distros as the default RMW.
set -euo pipefail

apt-get update
apt-get install -y "ros-${ROS_DISTRO}-rmw-fastrtps-cpp"

cat > /etc/profile.d/latency_rmw.sh <<'EOF'
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
EOF
chmod +x /etc/profile.d/latency_rmw.sh
