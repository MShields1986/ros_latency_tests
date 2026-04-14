#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <ros/ros.h>
#include <ros/serialization.h>
#include <nodelet/nodelet.h>

#include <latency_tests_msgs/LatencyRecord.h>

#include "latency_tests/payload_traits.hpp"

namespace latency_tests
{

template <typename MsgT>
class LatencyPublisher : public nodelet::Nodelet
{
public:
    void onInit() override
    {
        ros::NodeHandle nh  = getMTNodeHandle();
        ros::NodeHandle pnh = getMTPrivateNodeHandle();

        pnh.param<std::string>("pipeline_id",   pipeline_id_,   "run");
        pnh.param<std::string>("message_type",  message_type_,  "unknown");
        pnh.param<std::string>("output_topic",  output_topic_,  "/chain/hop_0");
        pnh.param<std::string>("records_topic", records_topic_, "/latency/records");
        pnh.param<int>("node_index", node_index_, 0);
        int requested_payload = 1024;
        pnh.param<int>("payload_bytes", requested_payload, 1024);
        pnh.param<double>("publish_rate_hz", publish_rate_hz_, 100.0);
        double warmup_s = 2.0;
        pnh.param<double>("warmup_s", warmup_s, 2.0);

        pub_         = nh.advertise<MsgT>(output_topic_, 100);
        records_pub_ = nh.advertise<latency_tests_msgs::LatencyRecord>(records_topic_, 10000);

        payload_traits<MsgT>::fill(template_msg_, static_cast<std::size_t>(requested_payload));
        actual_payload_bytes_ = ros::serialization::serializationLength(template_msg_);

        const double period = 1.0 / publish_rate_hz_;
        start_timer_ = nh.createTimer(
            ros::Duration(warmup_s),
            [this, period, nh](const ros::TimerEvent &) mutable {
                start_timer_.stop();
                timer_ = nh.createTimer(
                    ros::Duration(period),
                    &LatencyPublisher::tick, this);
            },
            true /*oneshot*/);

        NODELET_INFO("LatencyPublisher[%s] -> %s @ %.1f Hz, payload=%zu B (requested %d B)",
            message_type_.c_str(), output_topic_.c_str(),
            publish_rate_hz_, actual_payload_bytes_, requested_payload);
    }

private:
    void tick(const ros::TimerEvent &)
    {
        boost::shared_ptr<MsgT> msg(new MsgT(template_msg_));
        const ros::Time t_now = ros::Time::now();
        payload_traits<MsgT>::set_origin_stamp(*msg, t_now);
        payload_traits<MsgT>::set_correlation(*msg, pipeline_id_, seq_);

        pub_.publish(msg);

        latency_tests_msgs::LatencyRecord rec;
        rec.pipeline_id = pipeline_id_;
        rec.seq = seq_;
        rec.node_index = node_index_;
        rec.node_role = "publisher";
        rec.message_type = message_type_;
        rec.payload_bytes = static_cast<uint32_t>(actual_payload_bytes_);
        rec.t_origin = t_now;
        rec.t_observed = t_now;
        records_pub_.publish(rec);

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
    ros::Publisher pub_;
    ros::Publisher records_pub_;
    ros::Timer start_timer_;
    ros::Timer timer_;
};

}  // namespace latency_tests
