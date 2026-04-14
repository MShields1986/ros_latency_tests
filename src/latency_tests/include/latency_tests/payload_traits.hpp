#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include <boost/optional.hpp>
#include <ros/time.h>

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


template <typename T>
struct payload_traits
{
    static constexpr bool has_header = detail::has_header_field_v<T>;

    static void fill(T & /*msg*/, std::size_t /*target_payload_bytes*/) {}

    static void set_origin_stamp(T & msg, const ros::Time & t)
    {
        if constexpr (has_header) {
            msg.header.stamp = t;
        }
    }

    static boost::optional<ros::Time> get_origin_stamp(const T & msg)
    {
        if constexpr (has_header) {
            return msg.header.stamp;
        } else {
            (void)msg;
            return boost::none;
        }
    }

    static void set_correlation(T & msg, const std::string & pipeline_id, uint64_t seq)
    {
        if constexpr (has_header) {
            msg.header.frame_id = pipeline_id + ":" + std::to_string(seq);
        } else {
            (void)msg; (void)pipeline_id; (void)seq;
        }
    }

    static boost::optional<uint64_t> get_seq(const T & msg)
    {
        if constexpr (has_header) {
            const std::string & f = msg.header.frame_id;
            const auto colon = f.find(':');
            if (colon == std::string::npos) return boost::none;
            try {
                return std::stoull(f.substr(colon + 1));
            } catch (...) {
                return boost::none;
            }
        } else {
            (void)msg;
            return boost::none;
        }
    }
};

}  // namespace latency_tests

#include "latency_tests/payload_traits_specializations.hpp"
