#pragma once

#include <memory>
#include <string>

#include <ros/ros.h>
#include <nodelet/nodelet.h>

#include <latency_tests_msgs/LatencyRecord.h>

#include "latency_tests/payload_traits.hpp"
#include "latency_tests/transport_hints.hpp"

namespace latency_tests
{

template <typename MsgT>
class LatencySubscriber : public nodelet::Nodelet
{
public:
    void onInit() override
    {
        ros::NodeHandle nh  = getMTNodeHandle();
        ros::NodeHandle pnh = getMTPrivateNodeHandle();

        pnh.param<std::string>("pipeline_id",   pipeline_id_,   "run");
        pnh.param<std::string>("message_type",  message_type_,  "unknown");
        pnh.param<std::string>("input_topic",   input_topic_,   "/chain/hop_in");
        pnh.param<std::string>("records_topic", records_topic_, "/latency/records");
        pnh.param<int>("node_index", node_index_, 99);
        pnh.param<int>("payload_bytes", payload_bytes_, 0);
        std::string transport;
        pnh.param<std::string>("transport", transport, "tcp");

        records_pub_ = nh.advertise<latency_tests_msgs::LatencyRecord>(records_topic_, 10000);

        sub_ = nh.subscribe<MsgT>(
            input_topic_, 100,
            boost::bind(&LatencySubscriber::on_msg, this, _1),
            ros::VoidConstPtr(),
            make_hints(transport));

        NODELET_INFO("LatencySubscriber[%d] <- %s (transport=%s)",
            node_index_, input_topic_.c_str(), transport.c_str());
    }

private:
    void on_msg(const boost::shared_ptr<MsgT const> & msg)
    {
        const ros::Time t_now = ros::Time::now();

        latency_tests_msgs::LatencyRecord rec;
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
        records_pub_.publish(rec);
    }

    std::string pipeline_id_;
    std::string message_type_;
    std::string input_topic_;
    std::string records_topic_;
    int         node_index_{99};
    int         payload_bytes_{0};
    uint64_t    local_seq_{0};

    ros::Subscriber sub_;
    ros::Publisher  records_pub_;
};

}  // namespace latency_tests
