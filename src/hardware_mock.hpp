#pragma once

#include <cmath>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "types.hpp"

namespace jcan {

struct mock_adapter {
  bool open_{false};
  uint32_t seq_{0};
  can_frame::clock::time_point start_time_{};

  static constexpr int k_target_rate = 10000;
  static constexpr int k_batch_size = 100;

  [[nodiscard]] result<> open(
      const std::string&,
      [[maybe_unused]] slcan_bitrate bitrate = slcan_bitrate::s6,
      [[maybe_unused]] unsigned baud = 0) {
    if (open_) return std::unexpected(error_code::already_open);
    open_ = true;
    seq_ = 0;
    start_time_ = can_frame::clock::now();
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
      [[maybe_unused]] unsigned timeout_ms = 100) {
    if (!open_) return std::unexpected(error_code::not_open);

    static constexpr uint32_t demo_ids[] = {
        0x100, 0x200, 0x310, 0x400, 0x500, 0x600, 0x7DF, 0x123,
    };
    static constexpr int n_ids =
        static_cast<int>(sizeof(demo_ids) / sizeof(demo_ids[0]));

    auto now = can_frame::clock::now();
    double elapsed = std::chrono::duration<double>(now - start_time_).count();
    uint32_t target_seq = static_cast<uint32_t>(elapsed * k_target_rate);

    if (seq_ >= target_seq) {
      auto wake = start_time_ + std::chrono::duration_cast<can_frame::clock::duration>(
                                    std::chrono::duration<double>(
                                        static_cast<double>(seq_ + 1) / k_target_rate));
      std::this_thread::sleep_until(wake);
      now = can_frame::clock::now();
      elapsed = std::chrono::duration<double>(now - start_time_).count();
      target_seq = static_cast<uint32_t>(elapsed * k_target_rate);
    }

    uint32_t to_produce = std::min(target_seq - seq_,
                                   static_cast<uint32_t>(k_batch_size));

    std::vector<can_frame> frames;
    frames.reserve(to_produce);

    for (uint32_t b = 0; b < to_produce; ++b) {
      can_frame f{};
      f.timestamp = now;
      f.id = demo_ids[seq_ % n_ids];
      f.dlc = 8;

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

struct mock_fd_adapter {
  bool open_{false};
  uint64_t seq_{0};
  can_frame::clock::time_point start_time_{};

  static constexpr int k_target_rate = 2000;
  static constexpr int k_batch_size = 20;

  [[nodiscard]] result<> open(
      const std::string&,
      [[maybe_unused]] slcan_bitrate bitrate = slcan_bitrate::s6,
      [[maybe_unused]] unsigned baud = 0) {
    if (open_) return std::unexpected(error_code::already_open);
    open_ = true;
    seq_ = 0;
    start_time_ = can_frame::clock::now();
    return {};
  }

  [[nodiscard]] result<> close() {
    if (!open_) return std::unexpected(error_code::not_open);
    open_ = false;
    return {};
  }

  [[nodiscard]] result<> send(const can_frame&) {
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
      [[maybe_unused]] unsigned timeout_ms = 100) {
    if (!open_) return std::unexpected(error_code::not_open);

    static constexpr uint32_t fd_ids[] = {0x18DA00FA, 0x18DB33F1, 0x0CF004FE,
                                          0x18FEF100, 0x0CFF0003};
    static constexpr uint8_t fd_dlcs[] = {12, 16, 24, 32, 64};
    static constexpr int n_ids =
        static_cast<int>(sizeof(fd_ids) / sizeof(fd_ids[0]));

    auto now = can_frame::clock::now();
    double elapsed = std::chrono::duration<double>(now - start_time_).count();
    uint64_t target_seq = static_cast<uint64_t>(elapsed * k_target_rate);

    if (seq_ >= target_seq) {
      auto wake = start_time_ + std::chrono::duration_cast<can_frame::clock::duration>(
                                    std::chrono::duration<double>(
                                        static_cast<double>(seq_ + 1) / k_target_rate));
      std::this_thread::sleep_until(wake);
      now = can_frame::clock::now();
      elapsed = std::chrono::duration<double>(now - start_time_).count();
      target_seq = static_cast<uint64_t>(elapsed * k_target_rate);
    }

    uint64_t to_produce = std::min(target_seq - seq_,
                                   static_cast<uint64_t>(k_batch_size));

    std::vector<can_frame> frames;
    frames.reserve(static_cast<std::size_t>(to_produce));

    for (uint64_t b = 0; b < to_produce; ++b) {
      can_frame f{};
      f.timestamp = now;
      f.id = fd_ids[seq_ % n_ids];
      f.extended = true;
      f.fd = true;
      f.brs = true;
      f.dlc = len_to_dlc(fd_dlcs[seq_ % n_ids]);

      uint8_t payload_len = fd_dlcs[seq_ % n_ids];
      double t = static_cast<double>(seq_) * 0.001;
      for (uint8_t i = 0; i < payload_len; ++i) {
        double wave =
            std::sin(t * (1.0 + i * 0.3) + f.id * 0.05) * 127.0 + 128.0;
        f.data[i] = static_cast<uint8_t>(static_cast<int>(wave) & 0xFF);
      }

      ++seq_;
      frames.push_back(f);
    }
    return frames;
  }
};

struct mock_echo_adapter {
  struct shared_state {
    std::mutex mtx;
    std::vector<can_frame> pending;
  };

  bool open_{false};
  std::shared_ptr<shared_state> state_ = std::make_shared<shared_state>();

  [[nodiscard]] result<> open(
      const std::string&,
      [[maybe_unused]] slcan_bitrate bitrate = slcan_bitrate::s6,
      [[maybe_unused]] unsigned baud = 0) {
    if (open_) return std::unexpected(error_code::already_open);
    open_ = true;
    {
      std::lock_guard lk(state_->mtx);
      state_->pending.clear();
    }
    return {};
  }

  [[nodiscard]] result<> close() {
    if (!open_) return std::unexpected(error_code::not_open);
    open_ = false;
    return {};
  }

  [[nodiscard]] result<> send(const can_frame& frame) {
    if (!open_) return std::unexpected(error_code::not_open);
    std::lock_guard lk(state_->mtx);
    can_frame echo = frame;
    echo.timestamp = can_frame::clock::now();
    state_->pending.push_back(echo);
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
      [[maybe_unused]] unsigned timeout_ms = 100) {
    if (!open_) return std::unexpected(error_code::not_open);

    std::vector<can_frame> out;
    {
      std::lock_guard lk(state_->mtx);
      out.swap(state_->pending);
    }

    if (out.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return out;
  }
};

}  // namespace jcan
