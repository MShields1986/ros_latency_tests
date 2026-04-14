#!/usr/bin/env python3
"""Build and run a chained pub -> forwarder(s) -> subscriber nodelet pipeline
for ROS 1 Noetic transport latency benchmarking.

Transport selection:
    zerocopy     — all chain nodelets + collector loaded in a single nodelet
                   manager; publisher publishes boost::shared_ptr so subscribers
                   receive zero-copy pointers from the same manager.
    tcp          — each chain nodelet has its own nodelet manager (separate
                   process), so messages cross TCPROS between hops.
    tcp_nodelay  — like tcp, subscribers request TCP_NODELAY.
    udp          — like tcp, subscribers request UDPROS.

Args (ROS-style _key:=value):
    message_type               e.g. "sensor_msgs/PointCloud2"
    num_nodes                  total nodes in the chain (>=2)
    num_threads                manager worker threads (0 = hw concurrency)
    payload_bytes              target serialized payload size
    publish_rate_hz            publisher tick rate
    transport                  zerocopy | tcp | tcp_nodelay | udp
    pipeline_id                unique id for this run (default: timestamp)
    warmup_s                   seconds before publisher starts
    duration_s                 seconds of data to collect before shutdown
    output_dir                 CSV output directory inside the container
"""

import datetime as _dt
import os
import sys
import tempfile
import uuid

import roslaunch


_TYPE_TAGS = {
    "sensor_msgs/PointCloud2":        "sensor_msgs_PointCloud2",
    "sensor_msgs/Image":              "sensor_msgs_Image",
    "sensor_msgs/Imu":                "sensor_msgs_Imu",
    "sensor_msgs/LaserScan":          "sensor_msgs_LaserScan",
    "sensor_msgs/JointState":         "sensor_msgs_JointState",
    "sensor_msgs/PointCloud":         "sensor_msgs_PointCloud",
    "sensor_msgs/Range":              "sensor_msgs_Range",
    "sensor_msgs/Temperature":        "sensor_msgs_Temperature",
    "sensor_msgs/FluidPressure":      "sensor_msgs_FluidPressure",
    "nav_msgs/Odometry":              "nav_msgs_Odometry",
    "nav_msgs/Path":                  "nav_msgs_Path",
    "geometry_msgs/PointStamped":     "geometry_msgs_PointStamped",
    "geometry_msgs/PoseStamped":      "geometry_msgs_PoseStamped",
    "geometry_msgs/TwistStamped":     "geometry_msgs_TwistStamped",
    "geometry_msgs/WrenchStamped":    "geometry_msgs_WrenchStamped",
    "geometry_msgs/TransformStamped": "geometry_msgs_TransformStamped",
    "std_msgs/Header":                "std_msgs_Header",
    "std_msgs/String":                "std_msgs_String",
    "std_msgs/Bool":                  "std_msgs_Bool",
    "std_msgs/Byte":                  "std_msgs_Byte",
    "std_msgs/Char":                  "std_msgs_Char",
    "std_msgs/ColorRGBA":             "std_msgs_ColorRGBA",
    "std_msgs/Empty":                 "std_msgs_Empty",
    "std_msgs/Float32":               "std_msgs_Float32",
    "std_msgs/Float64":               "std_msgs_Float64",
    "std_msgs/Int8":                  "std_msgs_Int8",
    "std_msgs/Int16":                 "std_msgs_Int16",
    "std_msgs/Int32":                 "std_msgs_Int32",
    "std_msgs/Int64":                 "std_msgs_Int64",
    "std_msgs/UInt8":                 "std_msgs_UInt8",
    "std_msgs/UInt16":                "std_msgs_UInt16",
    "std_msgs/UInt32":                "std_msgs_UInt32",
    "std_msgs/UInt64":                "std_msgs_UInt64",
    "std_msgs/MultiArrayDimension":   "std_msgs_MultiArrayDimension",
    "std_msgs/MultiArrayLayout":      "std_msgs_MultiArrayLayout",
    "std_msgs/ByteMultiArray":        "std_msgs_ByteMultiArray",
    "std_msgs/Float32MultiArray":     "std_msgs_Float32MultiArray",
    "std_msgs/Float64MultiArray":     "std_msgs_Float64MultiArray",
    "std_msgs/Int8MultiArray":        "std_msgs_Int8MultiArray",
    "std_msgs/Int16MultiArray":       "std_msgs_Int16MultiArray",
    "std_msgs/Int32MultiArray":       "std_msgs_Int32MultiArray",
    "std_msgs/Int64MultiArray":       "std_msgs_Int64MultiArray",
    "std_msgs/UInt8MultiArray":       "std_msgs_UInt8MultiArray",
    "std_msgs/UInt16MultiArray":      "std_msgs_UInt16MultiArray",
    "std_msgs/UInt32MultiArray":      "std_msgs_UInt32MultiArray",
    "std_msgs/UInt64MultiArray":      "std_msgs_UInt64MultiArray",
}


def _parse_kv(argv):
    out = {}
    for a in argv:
        if ":=" in a:
            k, v = a.split(":=", 1)
            out[k] = v
    return out


def _bool(v, default=False):
    if v is None:
        return default
    return str(v).strip().lower() in ("1", "true", "yes", "on")


def _build_xml(args):
    message_type   = args.get("message_type", "sensor_msgs/PointCloud2")
    num_nodes      = int(args.get("num_nodes", 2))
    num_threads    = int(args.get("num_threads", 2))
    payload_bytes  = int(args.get("payload_bytes", 1024))
    publish_rate   = float(args.get("publish_rate_hz", 10.0))
    transport      = args.get("transport", "tcp")
    pipeline_id    = args.get("pipeline_id") or _dt.datetime.now().strftime("run_%Y%m%d_%H%M%S")
    warmup_s       = float(args.get("warmup_s", 10.0))
    duration_s     = float(args.get("duration_s", 600.0))
    output_dir     = args.get("output_dir", "/data/results")

    if num_nodes < 2:
        raise SystemExit("num_nodes must be >= 2")
    if transport not in ("zerocopy", "tcp", "tcp_nodelay", "udp"):
        raise SystemExit(f"invalid transport '{transport}'")
    tag = _TYPE_TAGS.get(message_type)
    if tag is None:
        raise SystemExit(
            f"unsupported message_type '{message_type}'. "
            f"Supported: {sorted(_TYPE_TAGS.keys())}"
        )

    pub_plug = f"latency_tests/LatencyPublisher_{tag}"
    fwd_plug = f"latency_tests/LatencyForwarder_{tag}"
    sub_plug = f"latency_tests/LatencySubscriber_{tag}"
    col_plug = "latency_tests/LatencyCollector"

    ipc = (transport == "zerocopy")

    common_params = (
        f'<param name="pipeline_id" value="{pipeline_id}"/>\n'
        f'<param name="message_type" value="{message_type}"/>\n'
        f'<param name="payload_bytes" value="{payload_bytes}"/>\n'
        f'<param name="transport" value="{transport}"/>\n'
    )

    def manager_node(name):
        return (
            f'<node pkg="nodelet" type="nodelet" name="{name}" '
            f'args="manager" output="screen" required="true">\n'
            f'  <param name="num_worker_threads" value="{num_threads}"/>\n'
            f'</node>\n'
        )

    def load_node(name, plugin, manager, params_xml):
        return (
            f'<node pkg="nodelet" type="nodelet" name="{name}" '
            f'args="load {plugin} {manager}" output="screen" required="true">\n'
            f'{params_xml}'
            f'</node>\n'
        )

    chain_params = []
    # Publisher
    pub_params = common_params + (
        f'<param name="node_index" value="0"/>\n'
        f'<param name="output_topic" value="/chain/hop_0"/>\n'
        f'<param name="publish_rate_hz" value="{publish_rate}"/>\n'
        f'<param name="warmup_s" value="{warmup_s}"/>\n'
    )
    chain_params.append(("publisher", pub_plug, pub_params))
    for i in range(1, num_nodes - 1):
        p = common_params + (
            f'<param name="node_index" value="{i}"/>\n'
            f'<param name="input_topic" value="/chain/hop_{i-1}"/>\n'
            f'<param name="output_topic" value="/chain/hop_{i}"/>\n'
        )
        chain_params.append((f"forwarder_{i}", fwd_plug, p))
    sub_i = num_nodes - 1
    sub_params = common_params + (
        f'<param name="node_index" value="{sub_i}"/>\n'
        f'<param name="input_topic" value="/chain/hop_{sub_i - 1}"/>\n'
    )
    chain_params.append(("subscriber", sub_plug, sub_params))

    out = ['<launch>']
    if ipc:
        out.append(manager_node("latency_manager"))
        for name, plugin, params in chain_params:
            out.append(load_node(name, plugin, "latency_manager", params))
        out.append(load_node("latency_collector", col_plug, "latency_manager",
            common_params
            + f'<param name="num_nodes" value="{num_nodes}"/>\n'
            + f'<param name="num_threads" value="{num_threads}"/>\n'
            + f'<param name="publish_rate_hz" value="{publish_rate}"/>\n'
            + f'<param name="use_intra_process_comms" value="true"/>\n'
            + f'<param name="output_dir" value="{output_dir}"/>\n'))
    else:
        # One manager per chain nodelet (separate processes) + one for collector
        for name, plugin, params in chain_params:
            mgr = f"manager_{name}"
            out.append(manager_node(mgr))
            out.append(load_node(name, plugin, mgr, params))
        out.append(manager_node("manager_collector"))
        out.append(load_node("latency_collector", col_plug, "manager_collector",
            common_params
            + f'<param name="num_nodes" value="{num_nodes}"/>\n'
            + f'<param name="num_threads" value="{num_threads}"/>\n'
            + f'<param name="publish_rate_hz" value="{publish_rate}"/>\n'
            + f'<param name="use_intra_process_comms" value="false"/>\n'
            + f'<param name="output_dir" value="{output_dir}"/>\n'))

    out.append('</launch>')
    xml = "\n".join(out)
    return xml, pipeline_id, warmup_s + duration_s


def main():
    argv = [a for a in sys.argv[1:] if not a.startswith("__")]
    args = _parse_kv(argv)
    xml, pipeline_id, total_s = _build_xml(args)

    tmp = tempfile.NamedTemporaryFile(
        prefix="latency_pipeline_", suffix=".launch", delete=False, mode="w")
    tmp.write(xml)
    tmp.close()

    print(f"[latency_pipeline] pipeline_id={pipeline_id} transport={args.get('transport','tcp')} "
          f"msg={args.get('message_type')} nodes={args.get('num_nodes')} "
          f"payload={args.get('payload_bytes')}B total={total_s}s")
    print(f"[latency_pipeline] launch file: {tmp.name}")

    uuid_ = roslaunch.rlutil.get_or_generate_uuid(None, False)
    roslaunch.configure_logging(uuid_)
    parent = roslaunch.parent.ROSLaunchParent(uuid_, [tmp.name])
    parent.start()

    import time
    try:
        deadline = time.time() + total_s
        while time.time() < deadline and parent.pm and not parent.pm.done:
            time.sleep(0.5)
    except KeyboardInterrupt:
        pass
    finally:
        print("[latency_pipeline] duration elapsed, shutting down")
        parent.shutdown()
        try:
            os.unlink(tmp.name)
        except OSError:
            pass


if __name__ == "__main__":
    main()
