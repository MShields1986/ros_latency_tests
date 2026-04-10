"""Launch a chained pub -> forwarder(s) -> subscriber pipeline in a single
multi-threaded composable node container for latency benchmarking.

Launch arguments (all overridable from the CLI):
    message_type               e.g. "sensor_msgs/PointCloud2"
    num_nodes                  total nodes in the chain (>=2)
    num_threads                executor threads inside the container (0 = hw concurrency)
    payload_bytes              target serialized payload size for variable-sized fields
    publish_rate_hz            publisher tick rate
    use_intra_process_comms    enable rclcpp intra-process comms inside the container
    pipeline_id                unique id for this run (default: timestamp)
    warmup_s                   seconds before publisher starts
    duration_s                 seconds before the launch shuts itself down
    output_dir                 directory (inside container) to write CSVs into
"""

import datetime as _dt

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    LogInfo,
    OpaqueFunction,
    Shutdown,
    TimerAction,
)
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


# Map a canonical "pkg/TypeName" string onto the registration tag used by the
# LATENCY_REGISTER_COMPONENTS macro. Extend this table alongside the C++
# component files.
_TYPE_TAGS = {
    "sensor_msgs/PointCloud2":         "sensor_msgs_PointCloud2",
    "sensor_msgs/Image":               "sensor_msgs_Image",
    "sensor_msgs/Imu":                 "sensor_msgs_Imu",
    "sensor_msgs/LaserScan":           "sensor_msgs_LaserScan",
    "sensor_msgs/JointState":          "sensor_msgs_JointState",
    "sensor_msgs/PointCloud":          "sensor_msgs_PointCloud",
    "sensor_msgs/Range":               "sensor_msgs_Range",
    "sensor_msgs/Temperature":         "sensor_msgs_Temperature",
    "sensor_msgs/FluidPressure":       "sensor_msgs_FluidPressure",
    "nav_msgs/Odometry":               "nav_msgs_Odometry",
    "nav_msgs/Path":                   "nav_msgs_Path",
    "geometry_msgs/PointStamped":      "geometry_msgs_PointStamped",
    "geometry_msgs/PoseStamped":       "geometry_msgs_PoseStamped",
    "geometry_msgs/TwistStamped":      "geometry_msgs_TwistStamped",
    "geometry_msgs/WrenchStamped":     "geometry_msgs_WrenchStamped",
    "geometry_msgs/TransformStamped":  "geometry_msgs_TransformStamped",
    "std_msgs/Header":                 "std_msgs_Header",
    "std_msgs/String":                 "std_msgs_String",
    "std_msgs/Bool":                   "std_msgs_Bool",
    "std_msgs/Byte":                   "std_msgs_Byte",
    "std_msgs/Char":                   "std_msgs_Char",
    "std_msgs/ColorRGBA":              "std_msgs_ColorRGBA",
    "std_msgs/Empty":                  "std_msgs_Empty",
    "std_msgs/Float32":                "std_msgs_Float32",
    "std_msgs/Float64":                "std_msgs_Float64",
    "std_msgs/Int8":                   "std_msgs_Int8",
    "std_msgs/Int16":                  "std_msgs_Int16",
    "std_msgs/Int32":                  "std_msgs_Int32",
    "std_msgs/Int64":                  "std_msgs_Int64",
    "std_msgs/UInt8":                  "std_msgs_UInt8",
    "std_msgs/UInt16":                 "std_msgs_UInt16",
    "std_msgs/UInt32":                 "std_msgs_UInt32",
    "std_msgs/UInt64":                 "std_msgs_UInt64",
    "std_msgs/MultiArrayDimension":    "std_msgs_MultiArrayDimension",
    "std_msgs/MultiArrayLayout":       "std_msgs_MultiArrayLayout",
    "std_msgs/ByteMultiArray":         "std_msgs_ByteMultiArray",
    "std_msgs/Float32MultiArray":      "std_msgs_Float32MultiArray",
    "std_msgs/Float64MultiArray":      "std_msgs_Float64MultiArray",
    "std_msgs/Int8MultiArray":         "std_msgs_Int8MultiArray",
    "std_msgs/Int16MultiArray":        "std_msgs_Int16MultiArray",
    "std_msgs/Int32MultiArray":        "std_msgs_Int32MultiArray",
    "std_msgs/Int64MultiArray":        "std_msgs_Int64MultiArray",
    "std_msgs/UInt8MultiArray":        "std_msgs_UInt8MultiArray",
    "std_msgs/UInt16MultiArray":       "std_msgs_UInt16MultiArray",
    "std_msgs/UInt32MultiArray":       "std_msgs_UInt32MultiArray",
    "std_msgs/UInt64MultiArray":       "std_msgs_UInt64MultiArray",
}


def _plugin_names(message_type: str):
    tag = _TYPE_TAGS.get(message_type)
    if tag is None:
        raise RuntimeError(
            f"Unsupported message_type '{message_type}'. "
            f"Supported: {sorted(_TYPE_TAGS.keys())}"
        )
    return (
        f"latency_tests::LatencyPublisher_{tag}",
        f"latency_tests::LatencyForwarder_{tag}",
        f"latency_tests::LatencySubscriber_{tag}",
    )


def _parse_bool(s: str) -> bool:
    return s.strip().lower() in ("1", "true", "yes", "on")


def _build(context, *args, **kwargs):
    message_type = LaunchConfiguration("message_type").perform(context)
    num_nodes    = int(LaunchConfiguration("num_nodes").perform(context))
    num_threads  = int(LaunchConfiguration("num_threads").perform(context))
    payload_bytes = int(LaunchConfiguration("payload_bytes").perform(context))
    publish_rate_hz = float(LaunchConfiguration("publish_rate_hz").perform(context))
    use_intra_process_comms = _parse_bool(
        LaunchConfiguration("use_intra_process_comms").perform(context))
    pipeline_id  = LaunchConfiguration("pipeline_id").perform(context)
    warmup_s     = float(LaunchConfiguration("warmup_s").perform(context))
    duration_s   = float(LaunchConfiguration("duration_s").perform(context))
    output_dir   = LaunchConfiguration("output_dir").perform(context)

    if num_nodes < 2:
        raise RuntimeError("num_nodes must be >= 2 (publisher + subscriber)")

    if not pipeline_id:
        pipeline_id = _dt.datetime.now().strftime("run_%Y%m%d_%H%M%S")

    pub_plugin, fwd_plugin, sub_plugin = _plugin_names(message_type)

    common = {
        "pipeline_id": pipeline_id,
        "message_type": message_type,
        "payload_bytes": payload_bytes,
    }

    # Applied to every chain node so rclcpp intra-process comms propagate
    # end-to-end when the flag is on.
    chain_extra = [{"use_intra_process_comms": use_intra_process_comms}]

    nodes = []

    # Publisher — node_index 0, output on /chain/hop_0
    nodes.append(ComposableNode(
        package="latency_tests",
        plugin=pub_plugin,
        name="publisher",
        parameters=[{
            **common,
            "node_index": 0,
            "output_topic": "/chain/hop_0",
            "publish_rate_hz": publish_rate_hz,
            "warmup_s": warmup_s,
        }],
        extra_arguments=chain_extra,
    ))

    # Forwarders — node_index 1..num_nodes-2, each reading hop_{i-1} and writing hop_i
    for i in range(1, num_nodes - 1):
        nodes.append(ComposableNode(
            package="latency_tests",
            plugin=fwd_plugin,
            name=f"forwarder_{i}",
            parameters=[{
                **common,
                "node_index": i,
                "input_topic":  f"/chain/hop_{i-1}",
                "output_topic": f"/chain/hop_{i}",
            }],
            extra_arguments=chain_extra,
        ))

    # Subscriber — node_index num_nodes-1, final input is hop_{num_nodes-2}
    sub_index = num_nodes - 1
    nodes.append(ComposableNode(
        package="latency_tests",
        plugin=sub_plugin,
        name="subscriber",
        parameters=[{
            **common,
            "node_index": sub_index,
            "input_topic": f"/chain/hop_{sub_index - 1}",
        }],
        extra_arguments=chain_extra,
    ))

    # Collector — records the flag into CSV metadata; its own subscription
    # does not need intra-process comms.
    nodes.append(ComposableNode(
        package="latency_tests",
        plugin="latency_tests::LatencyCollector",
        name="latency_collector",
        parameters=[{
            **common,
            "num_nodes": num_nodes,
            "num_threads": num_threads,
            "publish_rate_hz": publish_rate_hz,
            "use_intra_process_comms": use_intra_process_comms,
            "output_dir": output_dir,
        }],
    ))

    container = ComposableNodeContainer(
        name="latency_container",
        namespace="",
        package="latency_tests",
        executable="latency_container_mt",
        composable_node_descriptions=nodes,
        parameters=[{"num_threads": num_threads}],
        output="screen",
        emulate_tty=True,
    )

    shutdown = TimerAction(
        period=duration_s + warmup_s,
        actions=[
            LogInfo(msg=f"[latency_pipeline] duration elapsed, shutting down"),
            Shutdown(reason="run duration complete"),
        ],
    )

    return [
        LogInfo(msg=(
            f"[latency_pipeline] msg={message_type} nodes={num_nodes} "
            f"threads={num_threads} payload={payload_bytes}B rate={publish_rate_hz}Hz "
            f"ipc={use_intra_process_comms} pipeline_id={pipeline_id}"
        )),
        container,
        shutdown,
    ]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument("message_type",     default_value="geometry_msgs/PoseStamped"),
        # DeclareLaunchArgument("message_type",     default_value="sensor_msgs/PointCloud2"),
        DeclareLaunchArgument("num_nodes",        default_value="2"),
        DeclareLaunchArgument("num_threads",      default_value="2"),
        DeclareLaunchArgument("payload_bytes",    default_value="1024"),
        # DeclareLaunchArgument("payload_bytes",    default_value="1048576"),
        # DeclareLaunchArgument("payload_bytes",    default_value="5242880"),
        DeclareLaunchArgument("publish_rate_hz",  default_value="10.0"),
        DeclareLaunchArgument("use_intra_process_comms", default_value="false"),
        DeclareLaunchArgument("pipeline_id",      default_value=""),
        DeclareLaunchArgument("warmup_s",         default_value="10.0"),
        DeclareLaunchArgument("duration_s",       default_value="600.0"),
        DeclareLaunchArgument("output_dir",       default_value="/data/results"),
        OpaqueFunction(function=_build),
    ])
