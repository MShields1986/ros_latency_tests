#!/usr/bin/env bash
# Placeholder for agnocast (Tier IV zero-copy shim) integration.
#
# agnocast is not an RMW — it intercepts rclcpp publish/subscribe at link
# time to provide zero-copy shared-memory delivery between nodes on the same
# host. A real integration requires:
#   * building the workspace against agnocast-patched rclcpp headers,
#   * LD_PRELOAD'ing the agnocast shim at runtime,
#   * running an agnocast daemon in the container.
#
# Until that work lands, fail loudly so selecting this service makes it
# obvious it isn't wired up, rather than silently falling back to a
# different transport.
set -euo pipefail

echo "[agnocast.sh] agnocast integration is not implemented yet." >&2
echo "[agnocast.sh] Fill in this script once the patched rclcpp build and" >&2
echo "[agnocast.sh] runtime shim are available." >&2
exit 1
