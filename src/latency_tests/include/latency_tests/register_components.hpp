#pragma once

#include "rclcpp_components/register_node_macro.hpp"

#include "latency_tests/latency_publisher.hpp"
#include "latency_tests/latency_forwarder.hpp"
#include "latency_tests/latency_subscriber.hpp"

// Declare per-type component subclasses and register them as plugins.
//
// Example:
//   LATENCY_REGISTER_COMPONENTS(sensor_msgs::msg::PointCloud2,
//                               sensor_msgs_PointCloud2)
//
// Produces component plugins:
//   latency_tests::LatencyPublisher_sensor_msgs_PointCloud2
//   latency_tests::LatencyForwarder_sensor_msgs_PointCloud2
//   latency_tests::LatencySubscriber_sensor_msgs_PointCloud2
#define LATENCY_REGISTER_COMPONENTS(MSG_TYPE, TAG)                                    \
    namespace latency_tests {                                                         \
    class LatencyPublisher_##TAG : public LatencyPublisher<MSG_TYPE> {                \
    public:                                                                           \
        explicit LatencyPublisher_##TAG(const rclcpp::NodeOptions & options)          \
        : LatencyPublisher<MSG_TYPE>(options) {}                                      \
    };                                                                                \
    class LatencyForwarder_##TAG : public LatencyForwarder<MSG_TYPE> {                \
    public:                                                                           \
        explicit LatencyForwarder_##TAG(const rclcpp::NodeOptions & options)          \
        : LatencyForwarder<MSG_TYPE>(options) {}                                      \
    };                                                                                \
    class LatencySubscriber_##TAG : public LatencySubscriber<MSG_TYPE> {              \
    public:                                                                           \
        explicit LatencySubscriber_##TAG(const rclcpp::NodeOptions & options)         \
        : LatencySubscriber<MSG_TYPE>(options) {}                                     \
    };                                                                                \
    }                                                                                 \
    RCLCPP_COMPONENTS_REGISTER_NODE(latency_tests::LatencyPublisher_##TAG)            \
    RCLCPP_COMPONENTS_REGISTER_NODE(latency_tests::LatencyForwarder_##TAG)            \
    RCLCPP_COMPONENTS_REGISTER_NODE(latency_tests::LatencySubscriber_##TAG)
