#!/usr/bin/env bash
# Install Eclipse Cyclone DDS RMW and make it the default.
set -euo pipefail

apt-get update
apt-get install -y "ros-${ROS_DISTRO}-rmw-cyclonedds-cpp"

cat > /etc/profile.d/latency_rmw.sh <<'EOF'
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
EOF
chmod +x /etc/profile.d/latency_rmw.sh
