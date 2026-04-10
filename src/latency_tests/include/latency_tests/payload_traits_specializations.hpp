#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "sensor_msgs/msg/point_cloud2.hpp"
#include "sensor_msgs/msg/image.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "sensor_msgs/msg/point_cloud.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "nav_msgs/msg/path.hpp"
#include "std_msgs/msg/string.hpp"
#include "std_msgs/msg/byte_multi_array.hpp"
#include "std_msgs/msg/float32_multi_array.hpp"
#include "std_msgs/msg/float64_multi_array.hpp"
#include "std_msgs/msg/int8_multi_array.hpp"
#include "std_msgs/msg/int16_multi_array.hpp"
#include "std_msgs/msg/int32_multi_array.hpp"
#include "std_msgs/msg/int64_multi_array.hpp"
#include "std_msgs/msg/u_int8_multi_array.hpp"
#include "std_msgs/msg/u_int16_multi_array.hpp"
#include "std_msgs/msg/u_int32_multi_array.hpp"
#include "std_msgs/msg/u_int64_multi_array.hpp"

namespace latency_tests
{

// Helper to grow a vector<uint8_t> to an exact byte count.
inline void resize_bytes(std::vector<uint8_t> & v, std::size_t bytes)
{
    v.assign(bytes, 0u);
}

template <>
inline void payload_traits<sensor_msgs::msg::PointCloud2>::fill(
    sensor_msgs::msg::PointCloud2 & msg, std::size_t target_payload_bytes)
{
    // Configure a minimal XYZ float32 layout, then size `data` to match.
    msg.height = 1;
    msg.is_bigendian = false;
    msg.is_dense = true;
    msg.point_step = 12;  // 3 * float32

    if (msg.fields.empty()) {
        msg.fields.resize(3);
        const char * names[] = {"x", "y", "z"};
        for (int i = 0; i < 3; ++i) {
            msg.fields[i].name = names[i];
            msg.fields[i].offset = static_cast<uint32_t>(i * 4);
            msg.fields[i].datatype = 7;  // FLOAT32
            msg.fields[i].count = 1;
        }
    }

    const std::size_t n_points = target_payload_bytes / msg.point_step;
    msg.width = static_cast<uint32_t>(n_points);
    msg.row_step = msg.point_step * msg.width;
    resize_bytes(msg.data, static_cast<std::size_t>(msg.row_step));
}

template <>
inline void payload_traits<sensor_msgs::msg::Image>::fill(
    sensor_msgs::msg::Image & msg, std::size_t target_payload_bytes)
{
    // Single-row mono8 with a width chosen to hit the target byte count.
    msg.encoding = "mono8";
    msg.is_bigendian = 0;
    msg.height = 1;
    msg.width = static_cast<uint32_t>(target_payload_bytes);
    msg.step = msg.width;
    resize_bytes(msg.data, target_payload_bytes);
}

template <>
inline void payload_traits<sensor_msgs::msg::LaserScan>::fill(
    sensor_msgs::msg::LaserScan & msg, std::size_t target_payload_bytes)
{
    const std::size_t n = target_payload_bytes / sizeof(float);
    msg.angle_min = -3.14159f;
    msg.angle_max =  3.14159f;
    msg.angle_increment = (n > 0) ? (6.28318f / static_cast<float>(n)) : 0.0f;
    msg.range_min = 0.0f;
    msg.range_max = 100.0f;
    msg.ranges.assign(n, 1.0f);
    msg.intensities.clear();
}

template <>
inline void payload_traits<sensor_msgs::msg::PointCloud>::fill(
    sensor_msgs::msg::PointCloud & msg, std::size_t target_payload_bytes)
{
    const std::size_t n = target_payload_bytes / (3 * sizeof(float));
    msg.points.assign(n, {});
    msg.channels.clear();
}

template <>
inline void payload_traits<sensor_msgs::msg::JointState>::fill(
    sensor_msgs::msg::JointState & msg, std::size_t target_payload_bytes)
{
    const std::size_t n = target_payload_bytes / sizeof(double);
    msg.name.clear();
    msg.position.assign(n, 0.0);
    msg.velocity.clear();
    msg.effort.clear();
}

template <>
inline void payload_traits<nav_msgs::msg::Path>::fill(
    nav_msgs::msg::Path & msg, std::size_t target_payload_bytes)
{
    // PoseStamped is ~56 bytes of pose data; use that as the divisor.
    const std::size_t per_pose = sizeof(double) * 7;
    const std::size_t n = target_payload_bytes / per_pose;
    msg.poses.assign(n, {});
}

template <>
inline void payload_traits<std_msgs::msg::String>::fill(
    std_msgs::msg::String & msg, std::size_t target_payload_bytes)
{
    msg.data.assign(target_payload_bytes, 'x');
}

// Generic resizer for std_msgs::msg::*MultiArray — sizes the `data` vector
// so its byte footprint matches the target. Works for any message whose
// `data` field is a std::vector of a trivial element type.
template <typename Msg>
inline void fill_multi_array(Msg & msg, std::size_t target_payload_bytes)
{
    using elem_t = typename std::remove_reference_t<decltype(msg.data)>::value_type;
    const std::size_t n = target_payload_bytes / sizeof(elem_t);
    msg.data.assign(n, elem_t{});
    msg.layout.dim.clear();
    msg.layout.data_offset = 0;
}

#define LATENCY_MULTIARRAY_FILL_SPEC(MSG_T)                                  \
    template <>                                                              \
    inline void payload_traits<MSG_T>::fill(                                 \
        MSG_T & msg, std::size_t target_payload_bytes)                       \
    {                                                                        \
        fill_multi_array(msg, target_payload_bytes);                         \
    }

LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::ByteMultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::Float32MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::Float64MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::Int8MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::Int16MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::Int32MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::Int64MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::UInt8MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::UInt16MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::UInt32MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::msg::UInt64MultiArray)

#undef LATENCY_MULTIARRAY_FILL_SPEC

}  // namespace latency_tests
