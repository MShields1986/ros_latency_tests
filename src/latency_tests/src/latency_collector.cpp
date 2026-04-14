#include "latency_tests/latency_collector.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

#include <sys/utsname.h>
#include <unistd.h>

#include <pluginlib/class_list_macros.h>

namespace latency_tests
{

namespace
{
std::string sanitise(const std::string & s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '/' || c == ':' || c == ' ') out.push_back('_');
        else out.push_back(c);
    }
    return out;
}

std::string env_or(const char * name, const char * fallback)
{
    const char * v = std::getenv(name);
    return v ? std::string(v) : std::string(fallback);
}

std::string timestamp_now()
{
    const auto now = std::chrono::system_clock::now();
    const auto t  = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return ss.str();
}

double percentile(std::vector<int64_t> & v, double p)
{
    if (v.empty()) return 0.0;
    const std::size_t idx = static_cast<std::size_t>(std::clamp(
        p * static_cast<double>(v.size() - 1), 0.0,
        static_cast<double>(v.size() - 1)));
    std::nth_element(v.begin(), v.begin() + idx, v.end());
    return static_cast<double>(v[idx]);
}

double mean(const std::vector<int64_t> & v)
{
    if (v.empty()) return 0.0;
    long double s = 0.0L;
    for (auto x : v) s += x;
    return static_cast<double>(s / v.size());
}

double stddev(const std::vector<int64_t> & v, double m)
{
    if (v.size() < 2) return 0.0;
    long double s = 0.0L;
    for (auto x : v) {
        const long double d = static_cast<long double>(x) - m;
        s += d * d;
    }
    return static_cast<double>(std::sqrt(s / (v.size() - 1)));
}

std::string read_first(const std::string & path, const std::string & key)
{
    std::ifstream f(path);
    if (!f) return "unknown";
    std::string line;
    while (std::getline(f, line)) {
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string k = line.substr(0, colon);
        while (!k.empty() && (k.back() == ' ' || k.back() == '\t')) k.pop_back();
        if (k != key) continue;
        std::string v = line.substr(colon + 1);
        while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
        return v;
    }
    return "unknown";
}

std::string read_os_pretty_name()
{
    std::ifstream f("/etc/os-release");
    if (!f) return "unknown";
    std::string line;
    while (std::getline(f, line)) {
        const std::string key = "PRETTY_NAME=";
        if (line.rfind(key, 0) != 0) continue;
        std::string v = line.substr(key.size());
        if (!v.empty() && v.front() == '"') v.erase(v.begin());
        if (!v.empty() && v.back()  == '"') v.pop_back();
        return v;
    }
    return "unknown";
}

std::string host_name()
{
    char buf[256] = {0};
    if (gethostname(buf, sizeof(buf) - 1) == 0) return std::string(buf);
    return "unknown";
}
}  // namespace


void LatencyCollector::onInit()
{
    ros::NodeHandle nh  = getMTNodeHandle();
    ros::NodeHandle pnh = getMTPrivateNodeHandle();

    pnh.param<std::string>("output_dir",   output_dir_,   "/data/results");
    pnh.param<std::string>("message_type", message_type_, "unknown");
    pnh.param<std::string>("pipeline_id",  pipeline_id_,  "run");
    pnh.param<std::string>("transport",    transport_,    "unknown");
    pnh.param<int>("num_nodes",    num_nodes_,    2);
    pnh.param<int>("num_threads",  num_threads_,  0);
    pnh.param<int>("payload_bytes", payload_bytes_, 0);
    pnh.param<double>("publish_rate_hz", publish_rate_hz_, 0.0);
    pnh.param<bool>("use_intra_process_comms", use_intra_process_comms_, false);
    std::string records_topic;
    pnh.param<std::string>("records_topic", records_topic, "/latency/records");

    hop_samples_.resize(num_nodes_);
    cumulative_samples_.resize(num_nodes_);

    sub_ = nh.subscribe<latency_tests_msgs::LatencyRecord>(
        records_topic, 10000, &LatencyCollector::on_record, this);

    NODELET_INFO("LatencyCollector waiting for first publisher sample before opening CSV");
}

LatencyCollector::~LatencyCollector()
{
    write_summary();
    if (csv_.is_open()) csv_.close();
    if (summary_csv_.is_open()) summary_csv_.close();
}

std::string LatencyCollector::build_filename_stem()
{
    std::ostringstream ss;
    ss << output_dir_ << "/latency_"
       << sanitise(transport_) << "_"
       << sanitise(message_type_) << "_"
       << num_nodes_ << "nodes_"
       << num_threads_ << "threads_"
       << payload_bytes_ << "B_"
       << static_cast<int>(publish_rate_hz_) << "hz_"
       << (use_intra_process_comms_ ? "ipc_" : "noipc_")
       << timestamp_now();
    return ss.str();
}

void LatencyCollector::open_files()
{
    std::error_code ec;
    std::filesystem::create_directories(output_dir_, ec);

    filename_stem_ = build_filename_stem();

    csv_.open(filename_stem_ + ".csv");
    summary_csv_.open(filename_stem_ + "_summary.csv");

    struct utsname uts{};
    (void)uname(&uts);
    const std::string cpu_model  = read_first("/proc/cpuinfo", "model name");
    const std::string cpu_cores  = std::to_string(std::thread::hardware_concurrency());
    const std::string os_pretty  = read_os_pretty_name();
    const std::string hostname_s = host_name();

    auto write_meta = [&](std::ofstream & f) {
        f << "# pipeline_id=" << pipeline_id_ << "\n"
          << "# rmw=" << transport_ << "\n"
          << "# message_type=" << message_type_ << "\n"
          << "# num_nodes=" << num_nodes_ << "\n"
          << "# num_threads=" << num_threads_ << "\n"
          << "# payload_bytes=" << payload_bytes_ << "\n"
          << "# publish_rate_hz=" << publish_rate_hz_ << "\n"
          << "# use_intra_process_comms=" << (use_intra_process_comms_ ? "true" : "false") << "\n"
          << "# ros_distro=" << env_or("ROS_DISTRO", "?") << "\n"
          << "# start_time=" << timestamp_now() << "\n"
          << "# host=" << hostname_s << "\n"
          << "# cpu_model=" << cpu_model << "\n"
          << "# cpu_cores=" << cpu_cores << "\n"
          << "# os=" << os_pretty << "\n"
          << "# kernel=" << uts.sysname << " " << uts.release << " " << uts.machine << "\n";
    };
    write_meta(csv_);
    write_meta(summary_csv_);

    csv_ << "seq,node_index,node_role,t_origin_ns,t_observed_ns,"
            "cumulative_ns,hop_ns,payload_bytes\n";
}

void LatencyCollector::on_record(
    const latency_tests_msgs::LatencyRecord::ConstPtr & msg)
{
    if (msg->pipeline_id != pipeline_id_) return;
    if (msg->node_index < 0 || msg->node_index >= num_nodes_) return;

    std::lock_guard<std::mutex> lk(mu_);

    if (!csv_.is_open()) {
        if (msg->node_index != 0) return;
        payload_bytes_ = static_cast<int>(msg->payload_bytes);
        open_files();
        NODELET_INFO("LatencyCollector opened %s (actual payload=%u B)",
            filename_stem_.c_str(), msg->payload_bytes);
    }

    auto & rec = pending_[msg->seq];
    if (rec.per_node.empty()) rec.per_node.resize(num_nodes_);

    auto & s = rec.per_node[msg->node_index];
    if (!s.seen) {
        s.seen = true;
        s.t_origin_ns   = msg->t_origin.toNSec();
        s.t_observed_ns = msg->t_observed.toNSec();
        s.payload_bytes = msg->payload_bytes;
        s.role          = msg->node_role;
        ++rec.seen_count;
    }

    if (rec.seen_count == num_nodes_) {
        maybe_flush(msg->seq, rec);
        pending_.erase(msg->seq);
    }
}

void LatencyCollector::maybe_flush(uint64_t seq, PipelineRecord & rec)
{
    const int64_t t_origin = rec.per_node[0].t_origin_ns;
    int64_t prev_observed = rec.per_node[0].t_observed_ns;

    for (int i = 0; i < num_nodes_; ++i) {
        const auto & s = rec.per_node[i];
        const int64_t cumulative = s.t_observed_ns - t_origin;
        const int64_t hop = (i == 0) ? 0 : (s.t_observed_ns - prev_observed);
        write_row(seq, i, s, cumulative, hop);

        cumulative_samples_[i].push_back(cumulative);
        if (i > 0) hop_samples_[i].push_back(hop);
        prev_observed = s.t_observed_ns;
    }
    e2e_samples_.push_back(
        rec.per_node[num_nodes_ - 1].t_observed_ns - t_origin);
}

void LatencyCollector::write_row(
    uint64_t seq, int node_index, const Sample & s,
    int64_t cumulative_ns, int64_t hop_ns)
{
    csv_ << seq << ','
         << node_index << ','
         << s.role << ','
         << s.t_origin_ns << ','
         << s.t_observed_ns << ','
         << cumulative_ns << ','
         << hop_ns << ','
         << s.payload_bytes << '\n';
}

void LatencyCollector::write_summary()
{
    if (!summary_csv_.is_open()) return;

    std::lock_guard<std::mutex> lk(mu_);
    dropped_ += static_cast<int64_t>(pending_.size());

    summary_csv_ << "metric,node_index,count,min_ns,mean_ns,p50_ns,"
                    "p95_ns,p99_ns,max_ns,stddev_ns\n";

    auto dump = [&](const std::string & label, int idx, std::vector<int64_t> v) {
        if (v.empty()) return;
        std::sort(v.begin(), v.end());
        const double m  = mean(v);
        const double sd = stddev(v, m);
        summary_csv_ << label << ',' << idx << ',' << v.size() << ','
                     << v.front() << ','
                     << m << ','
                     << percentile(v, 0.50) << ','
                     << percentile(v, 0.95) << ','
                     << percentile(v, 0.99) << ','
                     << v.back() << ','
                     << sd << '\n';
    };

    for (int i = 1; i < num_nodes_; ++i) {
        dump("hop", i, hop_samples_[i]);
    }
    for (int i = 0; i < num_nodes_; ++i) {
        dump("cumulative", i, cumulative_samples_[i]);
    }
    dump("e2e", num_nodes_ - 1, e2e_samples_);

    summary_csv_ << "# dropped=" << dropped_ << "\n";
}

}  // namespace latency_tests

PLUGINLIB_EXPORT_CLASS(latency_tests::LatencyCollector, nodelet::Nodelet)
