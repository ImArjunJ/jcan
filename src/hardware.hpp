#pragma once

#include <optional>
#include <variant>
#include <vector>

#include "hardware_mock.hpp"
#include "hardware_slcan.hpp"
#include "hardware_sock.hpp"
#include "types.hpp"

namespace jcan {

using adapter = std::variant<serial_slcan, socket_can, mock_adapter>;

[[nodiscard]] inline result<> adapter_open(
    adapter& a, const std::string& port,
    slcan_bitrate bitrate = slcan_bitrate::s6, unsigned baud = 115200) {
  return std::visit(
      [&](auto& drv) -> result<> { return drv.open(port, bitrate, baud); }, a);
}

[[nodiscard]] inline result<> adapter_close(adapter& a) {
  return std::visit([](auto& drv) -> result<> { return drv.close(); }, a);
}

[[nodiscard]] inline result<> adapter_send(adapter& a, const can_frame& frame) {
  return std::visit([&](auto& drv) -> result<> { return drv.send(frame); }, a);
}

[[nodiscard]] inline result<std::optional<can_frame>> adapter_recv(
    adapter& a, unsigned timeout_ms = 100) {
  return std::visit(
      [&](auto& drv) -> result<std::optional<can_frame>> {
        return drv.recv(timeout_ms);
      }, a);
}

[[nodiscard]] inline result<std::vector<can_frame>> adapter_recv_many(
    adapter& a, unsigned timeout_ms = 100) {
  return std::visit(
      [&](auto& drv) -> result<std::vector<can_frame>> {
        return drv.recv_many(timeout_ms);
      }, a);
}

[[nodiscard]] inline adapter make_adapter(const device_descriptor& desc) {
  switch (desc.kind) {
    case adapter_kind::serial_slcan: return serial_slcan{};
    case adapter_kind::socket_can:   return socket_can{};
    case adapter_kind::mock:         return mock_adapter{};
  }
  return mock_adapter{};
}

}
