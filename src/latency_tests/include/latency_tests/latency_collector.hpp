#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <ros/ros.h>
#include <nodelet/nodelet.h>

#include <latency_tests_msgs/LatencyRecord.h>

namespace latency_tests
{

class LatencyCollector : public nodelet::Nodelet
{
public:
    LatencyCollector() = default;
    ~LatencyCollector() override;

    void onInit() override;

private:
    struct Sample {
        int64_t t_origin_ns{0};
        int64_t t_observed_ns{0};
        uint32_t payload_bytes{0};
        std::string role;
        bool seen{false};
    };

    struct PipelineRecord {
        std::vector<Sample> per_node;
        int seen_count{0};
    };

    void on_record(const latency_tests_msgs::LatencyRecord::ConstPtr & msg);
    void maybe_flush(uint64_t seq, PipelineRecord & rec);
    void write_row(uint64_t seq, int node_index, const Sample & s,
                   int64_t cumulative_ns, int64_t hop_ns);
    void write_summary();
    void open_files();
    std::string build_filename_stem();

    std::string output_dir_;
    std::string transport_;
    std::string message_type_;
    std::string pipeline_id_;
    int         num_nodes_{0};
    int         num_threads_{0};
    int         payload_bytes_{0};
    double      publish_rate_hz_{0.0};
    bool        use_intra_process_comms_{false};
    std::string filename_stem_;

    std::mutex mu_;
    std::map<uint64_t, PipelineRecord> pending_;
    std::ofstream csv_;
    std::ofstream summary_csv_;

    std::vector<std::vector<int64_t>> hop_samples_;
    std::vector<std::vector<int64_t>> cumulative_samples_;
    std::vector<int64_t>              e2e_samples_;
    int64_t dropped_{0};

    ros::Subscriber sub_;
};

}  // namespace latency_tests
