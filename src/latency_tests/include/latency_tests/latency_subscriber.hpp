#pragma once

#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "latency_tests_msgs/msg/latency_record.hpp"

#include "latency_tests/payload_traits.hpp"

namespace latency_tests
{

template <typename MsgT>
class LatencySubscriber : public rclcpp::Node
{
public:
    explicit LatencySubscriber(const rclcpp::NodeOptions & options)
    : rclcpp::Node("latency_subscriber", options)
    {
        pipeline_id_   = this->declare_parameter<std::string>("pipeline_id", "run");
        message_type_  = this->declare_parameter<std::string>("message_type", "unknown");
        input_topic_   = this->declare_parameter<std::string>("input_topic", "/chain/hop_in");
        records_topic_ = this->declare_parameter<std::string>("records_topic", "/latency/records");
        node_index_    = this->declare_parameter<int>("node_index", 99);
        payload_bytes_ = this->declare_parameter<int>("payload_bytes", 0);

        rclcpp::QoS chain_qos(rclcpp::KeepLast(100));
        chain_qos.reliable();

        rclcpp::QoS records_qos(rclcpp::KeepLast(10000));
        records_qos.reliable();
        records_pub_ = this->create_publisher<latency_tests_msgs::msg::LatencyRecord>(
            records_topic_, records_qos);

        sub_ = this->create_subscription<MsgT>(
            input_topic_, chain_qos,
            [this](std::shared_ptr<const MsgT> msg) { this->on_msg(msg); });

        RCLCPP_INFO(this->get_logger(),
            "LatencySubscriber[%d] <- %s", node_index_, input_topic_.c_str());
    }

private:
    void on_msg(std::shared_ptr<const MsgT> msg)
    {
        const rclcpp::Time t_now = this->now();

        latency_tests_msgs::msg::LatencyRecord rec;
        rec.pipeline_id = pipeline_id_;
        if (const auto s = payload_traits<MsgT>::get_seq(*msg)) {
            rec.seq = *s;
        } else {
            rec.seq = local_seq_++;
        }
        rec.node_index = node_index_;
        rec.node_role = "subscriber";
        rec.message_type = message_type_;
        rec.payload_bytes = static_cast<uint32_t>(payload_bytes_);
        if (const auto o = payload_traits<MsgT>::get_origin_stamp(*msg)) {
            rec.t_origin = *o;
        } else {
            rec.t_origin = t_now;
        }
        rec.t_observed = t_now;
        records_pub_->publish(rec);
    }

    std::string pipeline_id_;
    std::string message_type_;
    std::string input_topic_;
    std::string records_topic_;
    int         node_index_{99};
    int         payload_bytes_{0};
    uint64_t    local_seq_{0};

    typename rclcpp::Subscription<MsgT>::SharedPtr sub_;
    rclcpp::Publisher<latency_tests_msgs::msg::LatencyRecord>::SharedPtr records_pub_;
};

}  // namespace latency_tests
