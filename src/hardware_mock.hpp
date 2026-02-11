#pragma once

#include <cmath>
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

  // ~10,000 msgs/sec: 100 frames per 10ms batch
  static constexpr int k_batch_size = 100;

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

    static constexpr uint32_t demo_ids[] = {
        0x100, 0x200, 0x310, 0x400, 0x500, 0x600, 0x7DF, 0x123,
    };
    static constexpr int n_ids =
        static_cast<int>(sizeof(demo_ids) / sizeof(demo_ids[0]));

    std::vector<can_frame> frames;
    frames.reserve(k_batch_size);
    auto now = can_frame::clock::now();

    for (int b = 0; b < k_batch_size; ++b) {
      can_frame f{};
      f.timestamp = now;
      f.id = demo_ids[seq_ % n_ids];
      f.dlc = 8;

      // Generate somewhat realistic varying data using sin waves
      double t = static_cast<double>(seq_) * 0.001;
      for (uint8_t i = 0; i < 8; ++i) {
        double wave =
            std::sin(t * (1.0 + i * 0.7) + f.id * 0.1) * 127.0 + 128.0;
        f.data[i] = static_cast<uint8_t>(static_cast<int>(wave) & 0xFF);
      }

      ++seq_;
      frames.push_back(f);
    }
    return frames;
  }
};

}  // namespace jcan
