#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>

#include "rclcpp/time.hpp"
#include "builtin_interfaces/msg/time.hpp"

namespace latency_tests
{

namespace detail
{

template <typename T, typename = void>
struct has_header_field : std::false_type {};

template <typename T>
struct has_header_field<T, std::void_t<decltype(std::declval<T>().header)>> : std::true_type {};

template <typename T>
inline constexpr bool has_header_field_v = has_header_field<T>::value;

}  // namespace detail


// Primary template — sensible defaults for fixed-size messages that either
// have or don't have a header. Specialisations override fill() where there
// is a variable-sized field that can be grown to a target payload size.
template <typename T>
struct payload_traits
{
    static constexpr bool has_header = detail::has_header_field_v<T>;

    // Default: do nothing. Specialise for types with sized fields.
    static void fill(T & /*msg*/, std::size_t /*target_payload_bytes*/) {}

    static void set_origin_stamp(T & msg, const rclcpp::Time & t)
    {
        if constexpr (has_header) {
            msg.header.stamp = t;
        }
    }

    static std::optional<rclcpp::Time> get_origin_stamp(const T & msg)
    {
        if constexpr (has_header) {
            return rclcpp::Time(msg.header.stamp, RCL_ROS_TIME);
        } else {
            (void)msg;
            return std::nullopt;
        }
    }

    // Sequence id is stuffed into header.frame_id as "<pipeline_id>:<seq>".
    static void set_correlation(T & msg, const std::string & pipeline_id, uint64_t seq)
    {
        if constexpr (has_header) {
            msg.header.frame_id = pipeline_id + ":" + std::to_string(seq);
        } else {
            (void)msg; (void)pipeline_id; (void)seq;
        }
    }

    static std::optional<uint64_t> get_seq(const T & msg)
    {
        if constexpr (has_header) {
            const std::string & f = msg.header.frame_id;
            const auto colon = f.find(':');
            if (colon == std::string::npos) return std::nullopt;
            try {
                return std::stoull(f.substr(colon + 1));
            } catch (...) {
                return std::nullopt;
            }
        } else {
            (void)msg;
            return std::nullopt;
        }
    }
};

}  // namespace latency_tests

// Specialisations for types with variable-sized fields.
#include "latency_tests/payload_traits_specializations.hpp"
