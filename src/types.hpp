#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <string>

namespace jcan {

struct can_frame {
  uint32_t id{};
  bool extended{false};
  bool rtr{false};
  bool error{false};
  uint8_t dlc{};
  std::array<uint8_t, 64> data{};
  bool fd{false};
  bool brs{false};
  bool tx{false};
  uint8_t source{0xff};

  using clock = std::chrono::steady_clock;
  clock::time_point timestamp{};
};

[[nodiscard]] constexpr uint8_t dlc_to_len(uint8_t dlc) noexcept {
  constexpr uint8_t map[] = {0, 1,  2,  3,  4,  5,  6,  7,
                             8, 12, 16, 20, 24, 32, 48, 64};
  return (dlc < 16) ? map[dlc] : 64;
}

[[nodiscard]] constexpr uint8_t len_to_dlc(uint8_t len) noexcept {
  if (len <= 8) return len;
  if (len <= 12) return 9;
  if (len <= 16) return 10;
  if (len <= 20) return 11;
  if (len <= 24) return 12;
  if (len <= 32) return 13;
  if (len <= 48) return 14;
  return 15;
}

[[nodiscard]] constexpr uint8_t frame_payload_len(const can_frame& f) noexcept {
  return f.fd ? dlc_to_len(f.dlc) : std::min(f.dlc, static_cast<uint8_t>(8));
}

enum class error_code : uint8_t {
  ok = 0,
  port_not_found,
  port_open_failed,
  port_config_failed,
  permission_denied,
  write_error,
  read_error,
  read_timeout,
  frame_parse_error,
  socket_error,
  interface_not_found,
  already_open,
  not_open,
  unknown,
};

[[nodiscard]] constexpr const char* to_string(error_code ec) noexcept {
  switch (ec) {
    case error_code::ok:
      return "ok";
    case error_code::port_not_found:
      return "port_not_found";
    case error_code::port_open_failed:
      return "port_open_failed";
    case error_code::port_config_failed:
      return "port_config_failed";
    case error_code::permission_denied:
      return "permission_denied";
    case error_code::write_error:
      return "write_error";
    case error_code::read_error:
      return "read_error";
    case error_code::read_timeout:
      return "read_timeout";
    case error_code::frame_parse_error:
      return "frame_parse_error";
    case error_code::socket_error:
      return "socket_error";
    case error_code::interface_not_found:
      return "interface_not_found";
    case error_code::already_open:
      return "already_open";
    case error_code::not_open:
      return "not_open";
    case error_code::unknown:
      return "unknown";
  }
  return "?";
}

template <typename T = void>
using result = std::expected<T, error_code>;

enum class adapter_kind : uint8_t {
  serial_slcan,
  socket_can,
  vector_xl,
  kvaser_usb,
  mock,
  unbound,
};

struct device_descriptor {
  adapter_kind kind{};
  std::string port;
  std::string friendly_name;
};

enum class slcan_bitrate : uint8_t {
  s0 = 0,
  s1 = 1,
  s2 = 2,
  s3 = 3,
  s4 = 4,
  s5 = 5,
  s6 = 6,
  s7 = 7,
  s8 = 8,
};

}  // namespace jcan
