#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <sensor_msgs/PointCloud2.h>
#include <sensor_msgs/Image.h>
#include <sensor_msgs/LaserScan.h>
#include <sensor_msgs/PointCloud.h>
#include <sensor_msgs/JointState.h>
#include <nav_msgs/Path.h>
#include <std_msgs/String.h>
#include <std_msgs/ByteMultiArray.h>
#include <std_msgs/Float32MultiArray.h>
#include <std_msgs/Float64MultiArray.h>
#include <std_msgs/Int8MultiArray.h>
#include <std_msgs/Int16MultiArray.h>
#include <std_msgs/Int32MultiArray.h>
#include <std_msgs/Int64MultiArray.h>
#include <std_msgs/UInt8MultiArray.h>
#include <std_msgs/UInt16MultiArray.h>
#include <std_msgs/UInt32MultiArray.h>
#include <std_msgs/UInt64MultiArray.h>

namespace latency_tests
{

inline void resize_bytes(std::vector<uint8_t> & v, std::size_t bytes)
{
    v.assign(bytes, 0u);
}

template <>
inline void payload_traits<sensor_msgs::PointCloud2>::fill(
    sensor_msgs::PointCloud2 & msg, std::size_t target_payload_bytes)
{
    msg.height = 1;
    msg.is_bigendian = false;
    msg.is_dense = true;
    msg.point_step = 12;

    if (msg.fields.empty()) {
        msg.fields.resize(3);
        const char * names[] = {"x", "y", "z"};
        for (int i = 0; i < 3; ++i) {
            msg.fields[i].name = names[i];
            msg.fields[i].offset = static_cast<uint32_t>(i * 4);
            msg.fields[i].datatype = 7;
            msg.fields[i].count = 1;
        }
    }

    const std::size_t n_points = target_payload_bytes / msg.point_step;
    msg.width = static_cast<uint32_t>(n_points);
    msg.row_step = msg.point_step * msg.width;
    resize_bytes(msg.data, static_cast<std::size_t>(msg.row_step));
}

template <>
inline void payload_traits<sensor_msgs::Image>::fill(
    sensor_msgs::Image & msg, std::size_t target_payload_bytes)
{
    msg.encoding = "mono8";
    msg.is_bigendian = 0;
    msg.height = 1;
    msg.width = static_cast<uint32_t>(target_payload_bytes);
    msg.step = msg.width;
    resize_bytes(msg.data, target_payload_bytes);
}

template <>
inline void payload_traits<sensor_msgs::LaserScan>::fill(
    sensor_msgs::LaserScan & msg, std::size_t target_payload_bytes)
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
inline void payload_traits<sensor_msgs::PointCloud>::fill(
    sensor_msgs::PointCloud & msg, std::size_t target_payload_bytes)
{
    const std::size_t n = target_payload_bytes / (3 * sizeof(float));
    msg.points.assign(n, {});
    msg.channels.clear();
}

template <>
inline void payload_traits<sensor_msgs::JointState>::fill(
    sensor_msgs::JointState & msg, std::size_t target_payload_bytes)
{
    const std::size_t n = target_payload_bytes / sizeof(double);
    msg.name.clear();
    msg.position.assign(n, 0.0);
    msg.velocity.clear();
    msg.effort.clear();
}

template <>
inline void payload_traits<nav_msgs::Path>::fill(
    nav_msgs::Path & msg, std::size_t target_payload_bytes)
{
    const std::size_t per_pose = sizeof(double) * 7;
    const std::size_t n = target_payload_bytes / per_pose;
    msg.poses.assign(n, {});
}

template <>
inline void payload_traits<std_msgs::String>::fill(
    std_msgs::String & msg, std::size_t target_payload_bytes)
{
    msg.data.assign(target_payload_bytes, 'x');
}

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

LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::ByteMultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::Float32MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::Float64MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::Int8MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::Int16MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::Int32MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::Int64MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::UInt8MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::UInt16MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::UInt32MultiArray)
LATENCY_MULTIARRAY_FILL_SPEC(std_msgs::UInt64MultiArray)

#undef LATENCY_MULTIARRAY_FILL_SPEC

}  // namespace latency_tests
