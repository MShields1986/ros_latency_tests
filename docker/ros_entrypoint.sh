#!/bin/bash
set -e
source /opt/ros/noetic/setup.bash
source /catkin_ws/devel/setup.bash
# Start an in-container roscore on localhost if none is already running.
if ! rostopic list >/dev/null 2>&1; then
    roscore >/tmp/roscore.log 2>&1 &
    for _ in $(seq 1 30); do
        rostopic list >/dev/null 2>&1 && break
        sleep 0.2
    done
fi
exec "$@"
