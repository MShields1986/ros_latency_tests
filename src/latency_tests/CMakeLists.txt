cmake_minimum_required(VERSION 3.8)
project(latency_tests)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(
  THIS_PACKAGE_DEPS
    ament_cmake
    rclcpp
    rclcpp_components
    latency_tests_msgs
    std_msgs
    geometry_msgs
    nav_msgs
    sensor_msgs
)

foreach(PACKAGE ${THIS_PACKAGE_DEPS})
  find_package(${PACKAGE} REQUIRED)
endforeach()

include_directories(include)

set(COMMON_DEPS
  rclcpp
  rclcpp_components
  latency_tests_msgs
  std_msgs
  geometry_msgs
  nav_msgs
  sensor_msgs
)

# ----------------------------------------------------------------------------
# Per-type components library
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

add_library(latency_components SHARED ${COMPONENT_SOURCES})
ament_target_dependencies(latency_components ${COMMON_DEPS})
set_target_properties(latency_components PROPERTIES UNITY_BUILD ON)

# Register every class defined via LATENCY_REGISTER_COMPONENTS.
set(REGISTERED_TYPES
  sensor_msgs_PointCloud2
  sensor_msgs_Image
  sensor_msgs_Imu
  sensor_msgs_LaserScan
  sensor_msgs_JointState
  sensor_msgs_PointCloud
  sensor_msgs_Range
  sensor_msgs_Temperature
  sensor_msgs_FluidPressure
  nav_msgs_Odometry
  nav_msgs_Path
  geometry_msgs_PointStamped
  geometry_msgs_PoseStamped
  geometry_msgs_TwistStamped
  geometry_msgs_WrenchStamped
  geometry_msgs_TransformStamped
  std_msgs_Header
  std_msgs_String
  std_msgs_Bool
  std_msgs_Byte
  std_msgs_Char
  std_msgs_ColorRGBA
  std_msgs_Empty
  std_msgs_Float32
  std_msgs_Float64
  std_msgs_Int8
  std_msgs_Int16
  std_msgs_Int32
  std_msgs_Int64
  std_msgs_UInt8
  std_msgs_UInt16
  std_msgs_UInt32
  std_msgs_UInt64
  std_msgs_MultiArrayDimension
  std_msgs_MultiArrayLayout
  std_msgs_ByteMultiArray
  std_msgs_Float32MultiArray
  std_msgs_Float64MultiArray
  std_msgs_Int8MultiArray
  std_msgs_Int16MultiArray
  std_msgs_Int32MultiArray
  std_msgs_Int64MultiArray
  std_msgs_UInt8MultiArray
  std_msgs_UInt16MultiArray
  std_msgs_UInt32MultiArray
  std_msgs_UInt64MultiArray
)

foreach(TAG ${REGISTERED_TYPES})
  rclcpp_components_register_nodes(latency_components
    "latency_tests::LatencyPublisher_${TAG}"
    "latency_tests::LatencyForwarder_${TAG}"
    "latency_tests::LatencySubscriber_${TAG}"
  )
endforeach()

# ----------------------------------------------------------------------------
# Collector library + component
# ----------------------------------------------------------------------------
add_library(latency_collector_component SHARED src/latency_collector.cpp)
ament_target_dependencies(latency_collector_component ${COMMON_DEPS})
rclcpp_components_register_nodes(latency_collector_component
  "latency_tests::LatencyCollector"
)

# ----------------------------------------------------------------------------
# Custom multi-threaded component container executable
# ----------------------------------------------------------------------------
add_executable(latency_container_mt src/latency_container_mt.cpp)
ament_target_dependencies(latency_container_mt
  rclcpp
  rclcpp_components
)

# ----------------------------------------------------------------------------
# Install
# ----------------------------------------------------------------------------
install(TARGETS latency_components latency_collector_component
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(TARGETS latency_container_mt
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include
)

ament_package()
