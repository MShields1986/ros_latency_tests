# ros_latency_tests

Latency tests for different ROS 1 Noetic transports and message types, built
on `nodelet`.

A publisher emits stamped messages at a fixed rate into a chain of zero or
more forwarders, terminating at a subscriber. Every node publishes a
`latency_tests_msgs/LatencyRecord` on `/latency/records` as it processes each
message; a collector correlates them and writes per-hop + end-to-end CSVs
(plus a summary) to `data/results/`.

```mermaid
flowchart LR
    P([publisher<br/>node_index=0]):::pub
    F1([forwarder_1<br/>node_index=1]):::fwd
    F2([forwarder_2<br/>node_index=2]):::fwd
    S([subscriber<br/>node_index=N-1]):::sub
    C[[latency_collector<br/>CSV writer]]:::col

    P -- "/chain/hop_0" --> F1
    F1 -- "/chain/hop_1" --> F2
    F2 -- "... /chain/hop_{N-2}" --> S

    P -. "LatencyRecord" .-> C
    F1 -. "LatencyRecord" .-> C
    F2 -. "LatencyRecord" .-> C
    S -. "LatencyRecord" .-> C

    classDef pub fill:#cfe8ff,stroke:#1f6feb,color:#0a2540;
    classDef fwd fill:#fff3b0,stroke:#a07f00,color:#402c00;
    classDef sub fill:#d4f5d4,stroke:#1a7f1a,color:#0b3f0b;
    classDef col fill:#eeeeee,stroke:#555,color:#222;
```

`num_nodes=2` is pub → sub with no forwarders (single hop).
`num_nodes=5` is pub → fwd → fwd → fwd → sub (four hops).

Every pipeline node is a ROS 1 `nodelet::Nodelet`. The `transport` launch
arg selects how messages cross between hops:

| transport     | layout                                                          |
|---------------|-----------------------------------------------------------------|
| `zerocopy`    | all chain nodelets loaded into one `nodelet manager` → intra-process `boost::shared_ptr` pass through |
| `tcp`         | each nodelet has its own manager process → TCPROS (default)      |
| `tcp_nodelay` | each nodelet has its own manager; subscribers use `TransportHints().tcpNoDelay()` |
| `udp`         | each nodelet has its own manager; subscribers use `TransportHints().udp()`        |

## Quick start

```shell
git clone https://github.com/MShields1986/ros_latency_tests.git
cd ros_latency_tests

# Run one transport end-to-end (no plot):
./run_single.sh tcp

# Run every transport and then plot the distributions:
./run_all.sh
# override the list:
./run_all.sh "zerocopy tcp"
```

Results land in `data/results/` as
`latency_<transport>_<msg>_<N>nodes_<T>threads_<actual_payload>B_<rate>hz_<ipc|noipc>_<ts>.csv`,
each with a `#`-prefixed metadata header (pipeline config, intra-process flag,
host, CPU, OS, kernel). `run_all.sh` additionally emits a `violin_*.png` —
its title and subtitle are pulled from the CSV metadata.

## Knobs

Pass launch args after the service name via `docker compose run`, e.g.

```shell
docker compose -f docker/docker-compose.yaml run --rm latency \
  python3 /catkin_ws/src/latency_tests/launch/latency_pipeline.py \
    transport:=zerocopy \
    message_type:=sensor_msgs/PointCloud2 \
    payload_bytes:=1048576 \
    num_nodes:=5 \
    num_threads:=1 \
    publish_rate_hz:=100.0 \
    duration_s:=30.0
```

| launch arg        | default                   | notes |
|-------------------|---------------------------|-------|
| `message_type`    | `sensor_msgs/PointCloud2` | see list below |
| `num_nodes`       | `2`                       | pub + forwarders + sub, `>=2` |
| `num_threads`     | `2`                       | worker threads per nodelet manager (0 = hw concurrency) |
| `payload_bytes`   | `1048576`                 | target serialized size for variable-length fields |
| `publish_rate_hz` | `10.0`                    | publisher tick rate |
| `transport`       | `tcp`                     | `zerocopy` \| `tcp` \| `tcp_nodelay` \| `udp` |
| `warmup_s`        | `10.0`                    | delay before the publisher starts |
| `duration_s`      | `600.0`                   | seconds before the launch shuts itself down |

Supported `message_type` values come from `src/latency_tests/launch/latency_pipeline.py` (`_TYPE_TAGS`).
Adding a new one is one `.cpp` line in `src/latency_tests/src/components/` plus the tag in that map.

## Test matrix (`run_all.sh`)

```shell
MATRIX_TRANSPORTS="zerocopy tcp" \
MATRIX_MESSAGES="sensor_msgs/PointCloud2 sensor_msgs/Image" \
MATRIX_PAYLOADS="1024 1048576" \
MATRIX_NODES="2 5" \
MATRIX_THREADS="1 4" \
MATRIX_RATE=50.0 \
MATRIX_DURATION=30.0 \
./run_all.sh
```

Legacy form `./run_all.sh "tcp udp"` still works — the positional argument
replaces `MATRIX_TRANSPORTS`.

## Supported messages

`std_msgs`, `sensor_msgs`, `geometry_msgs`, `nav_msgs` — the same 46-entry set
as before (see `_TYPE_TAGS` in the launcher and `nodelets.xml`). Types with a
variable-length field sized to `payload_bytes`: `PointCloud2`, `Image`,
`LaserScan`, `PointCloud`, `JointState`, `Path`, `String`, and every
`*MultiArray` in `std_msgs`.

Note: `std_msgs` primitives have no `std_msgs/Header`, so they can't carry
the `pipeline_id:<seq>` correlation id through a chain. Forwarders fall back
to a local counter, which is fine for `num_nodes=2` (pub → sub) but can give
misleading hop numbers in longer chains if any drops occur. For multi-hop
runs prefer a type with a header.
