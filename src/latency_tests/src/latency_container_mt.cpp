#include <cstddef>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executors/multi_threaded_executor.hpp"
#include "rclcpp_components/component_manager.hpp"

int main(int argc, char ** argv)
{
    rclcpp::init(argc, argv);

    // The component manager itself is a regular node; we give it a name and
    // let it declare its own parameters, then read num_threads back out before
    // constructing the executor.
    rclcpp::NodeOptions node_opts;
    std::weak_ptr<rclcpp::Executor> empty_executor;
    auto manager = std::make_shared<rclcpp_components::ComponentManager>(
        empty_executor,
        "latency_container_mt",
        node_opts);

    int num_threads = manager->declare_parameter<int>("num_threads", 0);
    if (num_threads < 0) num_threads = 0;

    auto executor = std::make_shared<rclcpp::executors::MultiThreadedExecutor>(
        rclcpp::ExecutorOptions(),
        static_cast<std::size_t>(num_threads));

    manager->set_executor(executor);
    executor->add_node(manager);
    executor->spin();

    rclcpp::shutdown();
    return 0;
}
