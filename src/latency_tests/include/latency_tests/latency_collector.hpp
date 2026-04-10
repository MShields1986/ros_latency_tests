#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"
#include "latency_tests_msgs/msg/latency_record.hpp"

namespace latency_tests
{

class LatencyCollector : public rclcpp::Node
{
public:
    explicit LatencyCollector(const rclcpp::NodeOptions & options);
    ~LatencyCollector() override;

private:
    struct Sample {
        int64_t t_origin_ns{0};
        int64_t t_observed_ns{0};
        uint32_t payload_bytes{0};
        std::string role;
        bool seen{false};
    };

    struct PipelineRecord {
        std::vector<Sample> per_node;  // sized to num_nodes_
        int seen_count{0};
    };

    void on_record(const latency_tests_msgs::msg::LatencyRecord::SharedPtr msg);
    void maybe_flush(uint64_t seq, PipelineRecord & rec);
    void write_row(uint64_t seq, int node_index, const Sample & s,
                   int64_t cumulative_ns, int64_t hop_ns);
    void write_summary();
    void open_files();
    std::string build_filename_stem();

    // Config
    std::string output_dir_;
    std::string rmw_impl_;
    std::string message_type_;
    std::string pipeline_id_;
    int         num_nodes_{0};
    int         num_threads_{0};
    int         payload_bytes_{0};
    double      publish_rate_hz_{0.0};
    bool        use_intra_process_comms_{false};
    std::string filename_stem_;

    // State
    std::mutex mu_;
    std::map<uint64_t, PipelineRecord> pending_;
    std::ofstream csv_;
    std::ofstream summary_csv_;

    // Aggregate stats per node_index: hop_ns samples and cumulative_ns samples.
    std::vector<std::vector<int64_t>> hop_samples_;
    std::vector<std::vector<int64_t>> cumulative_samples_;
    std::vector<int64_t>              e2e_samples_;
    int64_t dropped_{0};

    rclcpp::Subscription<latency_tests_msgs::msg::LatencyRecord>::SharedPtr sub_;
};

}  // namespace latency_tests
