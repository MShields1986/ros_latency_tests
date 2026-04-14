FROM ros:noetic-ros-base

SHELL ["/bin/bash", "-c"]
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential \
      python3-catkin-tools \
      python3-rosdep \
      ros-noetic-nodelet \
      ros-noetic-pluginlib \
      ros-noetic-std-msgs \
      ros-noetic-sensor-msgs \
      ros-noetic-geometry-msgs \
      ros-noetic-nav-msgs \
      ros-noetic-message-generation \
      ros-noetic-message-runtime \
 && rm -rf /var/lib/apt/lists/*

# Build the workspace
COPY src /catkin_ws/src
WORKDIR /catkin_ws
RUN source /opt/ros/noetic/setup.bash \
 && rosdep update --rosdistro noetic \
 && rosdep install -i --from-path src --rosdistro noetic -y \
 && JOBS=$(( $(nproc) / 2 > 0 ? $(nproc) / 2 : 1 )) \
 && catkin_make -j"${JOBS}" -DCMAKE_BUILD_TYPE=Release install \
 && echo 'source /opt/ros/noetic/setup.bash'   >> /root/.bashrc \
 && echo 'source /catkin_ws/devel/setup.bash'  >> /root/.bashrc

COPY docker/ros_entrypoint.sh /ros_entrypoint.sh
RUN chmod +x /ros_entrypoint.sh
ENTRYPOINT ["/ros_entrypoint.sh"]
CMD ["bash"]

ENV DEBIAN_FRONTEND=dialog
