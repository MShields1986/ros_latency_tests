#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/serialization.hpp"
#include "rclcpp/serialized_message.hpp"
#include "latency_tests_msgs/msg/latency_record.hpp"

#include "latency_tests/payload_traits.hpp"

namespace latency_tests
{

namespace detail
{
template <typename MsgT>
std::size_t serialized_size(const MsgT & msg)
{
    rclcpp::Serialization<MsgT> serializer;
    rclcpp::SerializedMessage serialized;
    serializer.serialize_message(&msg, &serialized);
    return serialized.size();
}
}  // namespace detail

template <typename MsgT>
class LatencyPublisher : public rclcpp::Node
{
public:
    explicit LatencyPublisher(const rclcpp::NodeOptions & options)
    : rclcpp::Node("latency_publisher", options)
    {
        pipeline_id_    = this->declare_parameter<std::string>("pipeline_id", "run");
        message_type_   = this->declare_parameter<std::string>("message_type", "unknown");
        output_topic_   = this->declare_parameter<std::string>("output_topic", "/chain/hop_0");
        records_topic_  = this->declare_parameter<std::string>("records_topic", "/latency/records");
        node_index_     = this->declare_parameter<int>("node_index", 0);
        const int requested_payload = this->declare_parameter<int>("payload_bytes", 1024);
        publish_rate_hz_ = this->declare_parameter<double>("publish_rate_hz", 100.0);
        const double warmup_s = this->declare_parameter<double>("warmup_s", 2.0);

        rclcpp::QoS chain_qos(rclcpp::KeepLast(100));
        chain_qos.reliable();
        pub_ = this->create_publisher<MsgT>(output_topic_, chain_qos);

        rclcpp::QoS records_qos(rclcpp::KeepLast(10000));
        records_qos.reliable();
        records_pub_ = this->create_publisher<latency_tests_msgs::msg::LatencyRecord>(
            records_topic_, records_qos);

        // Fill template message (variable-size fields sized toward the request),
        // then serialise once to capture the true on-wire size.
        payload_traits<MsgT>::fill(template_msg_, static_cast<std::size_t>(requested_payload));
        actual_payload_bytes_ = detail::serialized_size(template_msg_);

        // Start after a warmup delay to let subscribers attach.
        const auto period = std::chrono::duration<double>(1.0 / publish_rate_hz_);
        start_timer_ = this->create_wall_timer(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::duration<double>(warmup_s)),
            [this, period]() {
                start_timer_->cancel();
                timer_ = this->create_wall_timer(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(period),
                    std::bind(&LatencyPublisher::tick, this));
            });

        RCLCPP_INFO(this->get_logger(),
            "LatencyPublisher[%s] -> %s @ %.1f Hz, payload=%zu B (requested %d B)",
            message_type_.c_str(), output_topic_.c_str(),
            publish_rate_hz_, actual_payload_bytes_, requested_payload);
    }

private:
    void tick()
    {
        MsgT msg = template_msg_;
        const rclcpp::Time t_now = this->now();
        payload_traits<MsgT>::set_origin_stamp(msg, t_now);
        payload_traits<MsgT>::set_correlation(msg, pipeline_id_, seq_);

        pub_->publish(msg);

        latency_tests_msgs::msg::LatencyRecord rec;
        rec.pipeline_id = pipeline_id_;
        rec.seq = seq_;
        rec.node_index = node_index_;
        rec.node_role = "publisher";
        rec.message_type = message_type_;
        rec.payload_bytes = static_cast<uint32_t>(actual_payload_bytes_);
        rec.t_origin = t_now;
        rec.t_observed = t_now;
        records_pub_->publish(rec);

        ++seq_;
    }

    std::string pipeline_id_;
    std::string message_type_;
    std::string output_topic_;
    std::string records_topic_;
    int         node_index_{0};
    std::size_t actual_payload_bytes_{0};
    double      publish_rate_hz_{100.0};
    uint64_t    seq_{0};

    MsgT template_msg_;
    typename rclcpp::Publisher<MsgT>::SharedPtr pub_;
    rclcpp::Publisher<latency_tests_msgs::msg::LatencyRecord>::SharedPtr records_pub_;
    rclcpp::TimerBase::SharedPtr start_timer_;
    rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace latency_tests
