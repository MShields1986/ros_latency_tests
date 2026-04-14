cmake_minimum_required(VERSION 3.0.2)
project(latency_tests)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  nodelet
  pluginlib
  latency_tests_msgs
  std_msgs
  geometry_msgs
  nav_msgs
  sensor_msgs
)

catkin_package(
  INCLUDE_DIRS include
  LIBRARIES latency_components latency_collector_component
  CATKIN_DEPENDS roscpp nodelet pluginlib latency_tests_msgs
                 std_msgs geometry_msgs nav_msgs sensor_msgs
)

include_directories(include ${catkin_INCLUDE_DIRS})

# ----------------------------------------------------------------------------
# Per-type nodelet library
# ----------------------------------------------------------------------------
set(COMPONENT_SOURCES
  src/components/sensor_msgs_point_cloud2.cpp
  src/components/sensor_msgs_image.cpp
  src/components/sensor_msgs_imu.cpp
  src/components/sensor_msgs_laser_scan.cpp
  src/components/sensor_msgs_joint_state.cpp
  src/components/sensor_msgs_point_cloud.cpp
  src/components/sensor_msgs_range.cpp
  src/components/sensor_msgs_temperature.cpp
  src/components/sensor_msgs_fluid_pressure.cpp
  src/components/nav_msgs_odometry.cpp
  src/components/nav_msgs_path.cpp
  src/components/geometry_msgs_point_stamped.cpp
  src/components/geometry_msgs_pose_stamped.cpp
  src/components/geometry_msgs_twist_stamped.cpp
  src/components/geometry_msgs_wrench_stamped.cpp
  src/components/geometry_msgs_transform_stamped.cpp
  src/components/std_msgs_header.cpp
  src/components/std_msgs_string.cpp
  src/components/std_msgs_bool.cpp
  src/components/std_msgs_byte.cpp
  src/components/std_msgs_char.cpp
  src/components/std_msgs_color_rgba.cpp
  src/components/std_msgs_empty.cpp
  src/components/std_msgs_float32.cpp
  src/components/std_msgs_float64.cpp
  src/components/std_msgs_int8.cpp
  src/components/std_msgs_int16.cpp
  src/components/std_msgs_int32.cpp
  src/components/std_msgs_int64.cpp
  src/components/std_msgs_u_int8.cpp
  src/components/std_msgs_u_int16.cpp
  src/components/std_msgs_u_int32.cpp
  src/components/std_msgs_u_int64.cpp
  src/components/std_msgs_multi_array_dimension.cpp
  src/components/std_msgs_multi_array_layout.cpp
  src/components/std_msgs_byte_multi_array.cpp
  src/components/std_msgs_float32_multi_array.cpp
  src/components/std_msgs_float64_multi_array.cpp
  src/components/std_msgs_int8_multi_array.cpp
  src/components/std_msgs_int16_multi_array.cpp
  src/components/std_msgs_int32_multi_array.cpp
  src/components/std_msgs_int64_multi_array.cpp
  src/components/std_msgs_u_int8_multi_array.cpp
  src/components/std_msgs_u_int16_multi_array.cpp
  src/components/std_msgs_u_int32_multi_array.cpp
  src/components/std_msgs_u_int64_multi_array.cpp
)

add_library(latency_components ${COMPONENT_SOURCES})
add_dependencies(latency_components ${catkin_EXPORTED_TARGETS})
target_link_libraries(latency_components ${catkin_LIBRARIES})

# ----------------------------------------------------------------------------
# Collector nodelet library
# ----------------------------------------------------------------------------
add_library(latency_collector_component src/latency_collector.cpp)
add_dependencies(latency_collector_component ${catkin_EXPORTED_TARGETS})
target_link_libraries(latency_collector_component ${catkin_LIBRARIES})

# ----------------------------------------------------------------------------
# Install
# ----------------------------------------------------------------------------
install(TARGETS latency_components latency_collector_component
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION}
)

install(DIRECTORY include/${PROJECT_NAME}/
  DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
)

install(DIRECTORY launch
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)

install(FILES nodelets.xml
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)
