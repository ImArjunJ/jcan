#pragma once

#include <cstdlib>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "types.hpp"

namespace jcan {

struct mock_adapter {
  bool open_{false};
  uint32_t seq_{0};

  [[nodiscard]] result<> open(
      const std::string&,
      [[maybe_unused]] slcan_bitrate bitrate = slcan_bitrate::s6,
      [[maybe_unused]] unsigned baud = 0) {
    if (open_) return std::unexpected(error_code::already_open);
    open_ = true;
    seq_ = 0;
    return {};
  }

  [[nodiscard]] result<> close() {
    if (!open_) return std::unexpected(error_code::not_open);
    open_ = false;
    return {};
  }

  [[nodiscard]] result<> send([[maybe_unused]] const can_frame& frame) {
    if (!open_) return std::unexpected(error_code::not_open);
    return {};
  }

  [[nodiscard]] result<std::optional<can_frame>> recv(
      unsigned timeout_ms = 100) {
    auto batch = recv_many(timeout_ms);
    if (!batch) return std::unexpected(batch.error());
    if (batch->empty()) return std::optional<can_frame>{std::nullopt};
    return std::optional<can_frame>{batch->front()};
  }

  [[nodiscard]] result<std::vector<can_frame>> recv_many(
      unsigned timeout_ms = 100) {
    if (!open_) return std::unexpected(error_code::not_open);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    (void)timeout_ms;

    std::vector<can_frame> frames;
    can_frame f{};
    f.timestamp = can_frame::clock::now();

    static constexpr uint32_t demo_ids[] = {0x100, 0x200, 0x310, 0x7DF};
    f.id = demo_ids[seq_ % 4];
    f.dlc = 8;
    for (uint8_t i = 0; i < 8; ++i)
      f.data[i] = static_cast<uint8_t>((seq_ + i) & 0xFF);

    ++seq_;
    frames.push_back(f);
    return frames;
  }
};

}
