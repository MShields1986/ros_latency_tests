#pragma once

#include <string>
#include <ros/transport_hints.h>

namespace latency_tests
{

// Build ros::TransportHints from a string name.
// "tcp_nodelay" -> TCP with TCP_NODELAY; "udp" -> UDPROS;
// anything else ("tcp", "zerocopy", empty) -> default TCPROS.
inline ros::TransportHints make_hints(const std::string & transport)
{
    if (transport == "tcp_nodelay") {
        return ros::TransportHints().tcpNoDelay();
    }
    if (transport == "udp") {
        return ros::TransportHints().udp();
    }
    return ros::TransportHints();
}

}  // namespace latency_tests
