#!/usr/bin/env bash
# Placeholder for iceoryx2 (v2) RMW integration.
#
# As of this writing there is no upstream `rmw_iceoryx2` package. iceoryx2
# is a ground-up Rust rewrite of iceoryx and does not yet ship a C++ RMW
# shim for ROS 2. When that lands, replace this script with the real
# install steps (apt package, from-source build, or vendored tarball as
# appropriate) and set RMW_IMPLEMENTATION accordingly.
#
# For now: fail loudly so anyone who selects this service knows it isn't
# wired up, rather than silently falling back to a different transport.
set -euo pipefail

echo "[iceoryx2.sh] iceoryx2 RMW integration is not implemented yet." >&2
echo "[iceoryx2.sh] Use the 'iceoryx' service for the v1 RMW, or fill" >&2
echo "[iceoryx2.sh] in this script once rmw_iceoryx2 becomes available." >&2
exit 1
