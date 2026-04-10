#!/usr/bin/env bash
# Fast DDS using the dynamic type-support variant (rmw_fastrtps_dynamic_cpp).
# Same underlying Fast DDS library as the `fastdds` service, different
# serialization path — useful for A/B'ing dynamic vs. static type-support.
set -euo pipefail

apt-get update
apt-get install -y "ros-${ROS_DISTRO}-rmw-fastrtps-dynamic-cpp"

cat > /etc/profile.d/latency_rmw.sh <<'EOF'
export RMW_IMPLEMENTATION=rmw_fastrtps_dynamic_cpp
EOF
chmod +x /etc/profile.d/latency_rmw.sh
