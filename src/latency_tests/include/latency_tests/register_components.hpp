#pragma once

#include <pluginlib/class_list_macros.h>

#include "latency_tests/latency_publisher.hpp"
#include "latency_tests/latency_forwarder.hpp"
#include "latency_tests/latency_subscriber.hpp"

// Declare per-type nodelet subclasses and export them via pluginlib.
//
// Example:
//   LATENCY_REGISTER_COMPONENTS(sensor_msgs::PointCloud2, sensor_msgs_PointCloud2)
//
// Produces nodelets:
//   latency_tests/LatencyPublisher_sensor_msgs_PointCloud2
//   latency_tests/LatencyForwarder_sensor_msgs_PointCloud2
//   latency_tests/LatencySubscriber_sensor_msgs_PointCloud2
#define LATENCY_REGISTER_COMPONENTS(MSG_TYPE, TAG)                                    \
    namespace latency_tests {                                                         \
    class LatencyPublisher_##TAG : public LatencyPublisher<MSG_TYPE> {};              \
    class LatencyForwarder_##TAG : public LatencyForwarder<MSG_TYPE> {};              \
    class LatencySubscriber_##TAG : public LatencySubscriber<MSG_TYPE> {};            \
    }                                                                                 \
    PLUGINLIB_EXPORT_CLASS(latency_tests::LatencyPublisher_##TAG,  nodelet::Nodelet)  \
    PLUGINLIB_EXPORT_CLASS(latency_tests::LatencyForwarder_##TAG,  nodelet::Nodelet)  \
    PLUGINLIB_EXPORT_CLASS(latency_tests::LatencySubscriber_##TAG, nodelet::Nodelet)
