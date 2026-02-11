#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "dbc_engine.hpp"
#include "frame_buffer.hpp"
#include "hardware.hpp"

struct ImFont;
#include "logger.hpp"
#include "permissions.hpp"
#include "signal_store.hpp"
#include "tx_scheduler.hpp"
#include "types.hpp"

namespace jcan {

struct bus_stats {
  using clock = std::chrono::steady_clock;

  struct id_stats {
    uint64_t total_count{0};
    uint64_t window_count{0};
    float rate_hz{0.f};
  };

  std::unordered_map<uint32_t, id_stats> per_id;
  uint64_t total_frames{0};
  uint64_t window_frames{0};
  double window_bits{0.0};
  float total_rate_hz{0.f};
  float bus_load_pct{0.f};
  clock::time_point window_start{clock::now()};

  uint64_t error_frames{0};
  uint64_t bus_off_count{0};
  uint64_t error_passive_count{0};
  uint8_t last_slcan_status{0};

  void update(float bitrate_bps) {
    auto now = clock::now();
    auto window = std::chrono::duration<double>(now - window_start).count();
    if (window < 0.001) return;

    total_rate_hz =
        static_cast<float>(static_cast<double>(window_frames) / window);

    for (auto& [id, st] : per_id) {
      st.rate_hz =
          static_cast<float>(static_cast<double>(st.window_count) / window);
    }

    bus_load_pct =
        (bitrate_bps > 0.f)
            ? static_cast<float>(window_bits / window /
                                 static_cast<double>(bitrate_bps) * 100.0)
            : 0.f;

    if (window > 3.0) {
      for (auto& [id, st] : per_id) st.window_count = 0;
      window_frames = 0;
      window_bits = 0.0;
      window_start = now;
    }
  }

  void record(const can_frame& f) {
    if (f.error) {
      error_frames++;
      return;
    }
    total_frames++;
    window_frames++;
    uint8_t payload_len = frame_payload_len(f);
    double frame_bits;
    if (f.fd) {
      frame_bits = (29.0 + payload_len * 8.0 + 21.0) * 1.1;
    } else {
      frame_bits = (47.0 + payload_len * 8.0) * 1.2;
    }
    window_bits += frame_bits;
    auto& st = per_id[f.id];
    st.total_count++;
    st.window_count++;
  }

  void record_slcan_status(uint8_t status) {
    last_slcan_status = status;
    if (status & 0x20) bus_off_count++;
    if (status & 0x04) error_passive_count++;
  }

  void reset() {
    per_id.clear();
    total_frames = 0;
    window_frames = 0;
    window_bits = 0.0;
    total_rate_hz = 0.f;
    bus_load_pct = 0.f;
    error_frames = 0;
    bus_off_count = 0;
    error_passive_count = 0;
    last_slcan_status = 0;
    window_start = clock::now();
  }
};

struct app_state {
  struct adapter_slot {
    device_descriptor desc;
    adapter hw;
    frame_buffer<8192> rx_buf;
    std::optional<std::jthread> io_thread;

    void start_io() {
      io_thread.emplace([this](std::stop_token stop) {
        while (!stop.stop_requested()) {
          auto result = adapter_recv_many(hw, 50);
          if (!result) {
            if (std::getenv("JCAN_DEBUG"))
              std::fprintf(stderr, "[io] recv error: %s\n",
                           to_string(result.error()));
            continue;
          }
          for (auto& f : *result) rx_buf.push(f);
        }
      });
    }
    void stop_io() { io_thread.reset(); }
  };

  std::vector<device_descriptor> devices;
  int selected_device{0};
  int selected_bitrate{6};
  std::vector<std::unique_ptr<adapter_slot>> adapter_slots;
  int tx_slot_idx{0};
  bool connected{false};
  std::string status_text{"Disconnected"};

  struct frame_row {
    can_frame frame;
    uint64_t count{1};
    float dt_ms{0.f};
    float sig_height{0.f};
  };
  std::vector<frame_row> monitor_rows;
  std::vector<frame_row> frozen_rows;
  std::vector<can_frame> scrollback;
  bool monitor_autoscroll{true};
  bool monitor_freeze{false};
  char filter_text[64]{};
  char scrollback_filter_text[64]{};
  static constexpr std::size_t k_max_scrollback = 50'000'000;

  bool show_connection_modal{true};
  float ui_scale{1.0f};
  bool show_signals{true};
  bool show_transmitter{true};
  bool show_statistics{true};
  bool show_plotter{true};
  ImFont* mono_font{nullptr};

  can_frame::clock::time_point first_frame_time{};
  bool has_first_frame{false};

  dbc_engine dbc;
  std::vector<std::string> dbc_paths;

  tx_scheduler tx_sched;

  frame_logger logger;
  signal_store signals;
  frame_buffer<8192> replay_buf;
  std::optional<std::jthread> replay_thread;
  std::atomic<bool> replaying{false};
  std::atomic<bool> replay_paused{false};
  std::atomic<float> replay_speed{1.0f};
  std::atomic<float> replay_progress{0.f};
  std::atomic<std::size_t> replay_total_frames{0};

  bus_stats stats;

  adapter* tx_adapter() {
    if (tx_slot_idx >= 0 &&
        tx_slot_idx < static_cast<int>(adapter_slots.size()))
      return &adapter_slots[static_cast<std::size_t>(tx_slot_idx)]->hw;
    return nullptr;
  }

  void connect() {
    if (devices.empty()) return;
    const auto& desc = devices[static_cast<std::size_t>(selected_device)];

    if (desc.kind == adapter_kind::unbound) {
      status_text = std::format("No driver loaded for {}", desc.friendly_name);
      return;
    }

    for (const auto& slot : adapter_slots) {
      if (slot->desc.port == desc.port) {
        status_text = std::format("Already connected: {}", desc.port);
        return;
      }
    }

    auto slot = std::make_unique<adapter_slot>();
    slot->desc = desc;
    slot->hw = make_adapter(desc);
    auto bitrate = static_cast<slcan_bitrate>(selected_bitrate);
    if (auto r = adapter_open(slot->hw, desc.port, bitrate); !r) {
      if (r.error() == error_code::permission_denied) {
        status_text =
            std::format("Permission denied: {} - requesting fix...", desc.port);
        if (install_udev_rule()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
          slot->hw = make_adapter(desc);
          if (auto r2 = adapter_open(slot->hw, desc.port, bitrate); !r2) {
            status_text = std::format("Still failed after fix: {}",
                                      to_string(r2.error()));
            return;
          }
          status_text = "Permissions fixed!";
        } else {
          status_text = "Permission fix cancelled.";
          return;
        }
      } else {
        status_text = std::format("Open failed: {}", to_string(r.error()));
        return;
      }
    }
    slot->start_io();
    adapter_slots.push_back(std::move(slot));
    connected = true;
    status_text =
        std::format("Connected: {} ({} adapter{})", desc.friendly_name,
                    adapter_slots.size(), adapter_slots.size() > 1 ? "s" : "");

    if (adapter_slots.size() == 1) {
      tx_slot_idx = 0;
      tx_sched.start(adapter_slots[0]->hw);
    }
  }

  void disconnect_slot(int idx) {
    if (idx < 0 || idx >= static_cast<int>(adapter_slots.size())) return;
    if (idx == tx_slot_idx) tx_sched.stop();
    adapter_slots[static_cast<std::size_t>(idx)]->stop_io();
    (void)adapter_close(adapter_slots[static_cast<std::size_t>(idx)]->hw);
    adapter_slots.erase(adapter_slots.begin() + idx);

    if (tx_slot_idx >= static_cast<int>(adapter_slots.size()))
      tx_slot_idx = std::max(0, static_cast<int>(adapter_slots.size()) - 1);

    if (adapter_slots.empty()) {
      connected = false;
      tx_sched.stop();
      logger.stop();
      status_text = "Disconnected";
    } else {
      if (!tx_sched.running())
        tx_sched.start(
            adapter_slots[static_cast<std::size_t>(tx_slot_idx)]->hw);
      status_text = std::format("{} adapter{} connected", adapter_slots.size(),
                                adapter_slots.size() > 1 ? "s" : "");
    }
  }

  void disconnect() {
    tx_sched.stop();
    logger.stop();
    for (auto& slot : adapter_slots) {
      slot->stop_io();
      (void)adapter_close(slot->hw);
    }
    adapter_slots.clear();
    connected = false;
    status_text = "Disconnected";
  }

  void poll_frames() {
    std::vector<can_frame> frames;
    for (std::size_t si = 0; si < adapter_slots.size(); ++si) {
      auto slot_frames = adapter_slots[si]->rx_buf.drain();
      for (auto& f : slot_frames) f.source = static_cast<uint8_t>(si);
      frames.insert(frames.end(), slot_frames.begin(), slot_frames.end());
    }
    auto replay_frames = replay_buf.drain();
    frames.insert(frames.end(), replay_frames.begin(), replay_frames.end());

    auto tx_frames = tx_sched.drain_sent();
    frames.insert(frames.end(), tx_frames.begin(), tx_frames.end());

    for (auto& f : frames) {
      if (!has_first_frame) {
        first_frame_time = f.timestamp;
        has_first_frame = true;
      }

      stats.record(f);

      if (f.error) continue;

      scrollback.push_back(f);
      if (scrollback.size() > k_max_scrollback)
        scrollback.erase(
            scrollback.begin(),
            scrollback.begin() +
                static_cast<long>(scrollback.size() - k_max_scrollback));
      logger.log(f);

      if (dbc.has_message(f.id)) {
        auto decoded = dbc.decode(f);
        for (const auto& sig : decoded) {
          signals.push(signal_key{.msg_id = f.id, .name = sig.name},
                       f.timestamp, sig.value, sig.unit, sig.minimum,
                       sig.maximum);
        }
      }

      if (!monitor_freeze) {
        bool found = false;
        for (auto& row : monitor_rows) {
          if (row.frame.id == f.id && row.frame.extended == f.extended &&
              row.frame.source == f.source) {
            auto dt = f.timestamp - row.frame.timestamp;
            row.dt_ms = std::chrono::duration<float, std::milli>(dt).count();
            row.frame = f;
            row.count++;
            found = true;
            break;
          }
        }
        if (!found) {
          monitor_rows.push_back({.frame = f, .count = 1, .dt_ms = 0.f});
        }
      }
    }
  }

  void toggle_freeze() {
    monitor_freeze = !monitor_freeze;
    if (monitor_freeze) {
      frozen_rows = monitor_rows;
    }
  }

  void start_replay(std::vector<std::pair<int64_t, can_frame>> frames) {
    stop_replay();
    replaying.store(true);
    replay_paused.store(false);
    replay_progress.store(0.f);
    replay_total_frames.store(frames.size());
    replay_thread.emplace([this,
                           frames = std::move(frames)](std::stop_token stop) {
      if (frames.empty()) {
        replaying.store(false);
        return;
      }
      auto base = can_frame::clock::now();
      int64_t first_ts = frames[0].first;
      auto logical_start = can_frame::clock::now();
      double logical_us = 0.0;

      for (std::size_t i = 0; i < frames.size() && !stop.stop_requested();
           ++i) {
        while (replay_paused.load() && !stop.stop_requested()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
          logical_start = can_frame::clock::now();
        }
        if (stop.stop_requested()) break;

        double target_us = static_cast<double>(frames[i].first - first_ts);
        float speed = replay_speed.load();
        if (speed < 0.1f) speed = 1.0f;

        double wait_us = (target_us - logical_us) / speed;
        if (wait_us > 0) {
          auto wake = logical_start +
                      std::chrono::microseconds(static_cast<int64_t>(wait_us));
          std::this_thread::sleep_until(wake);
        }

        logical_us = target_us;
        logical_start = can_frame::clock::now();

        auto f = frames[i].second;
        f.timestamp = can_frame::clock::now();
        replay_buf.push(f);
        replay_progress.store(static_cast<float>(i + 1) /
                              static_cast<float>(frames.size()));
      }
      replaying.store(false);
    });
  }

  void stop_replay() {
    replay_thread.reset();
    replaying.store(false);
    replay_paused.store(false);
    replay_progress.store(0.f);
  }

  void clear_monitor() {
    monitor_rows.clear();
    frozen_rows.clear();
    scrollback.clear();
    stats.reset();
    signals.clear();
    has_first_frame = false;
  }

  float import_log(std::vector<std::pair<int64_t, can_frame>> frames) {
    if (frames.empty()) return 0.f;
    clear_monitor();

    // Compute log duration
    int64_t first_ts = frames.front().first;
    int64_t last_ts = frames.back().first;
    double duration_sec = static_cast<double>(last_ts - first_ts) / 1e6;
    if (duration_sec < 0.1) duration_sec = 1.0;

    if (duration_sec > signals.max_seconds()) {
      signals.set_max_seconds(duration_sec * 1.1);
    }

    auto now = can_frame::clock::now();
    auto log_duration = std::chrono::microseconds(last_ts - first_ts);
    auto base_time = now - log_duration;

    for (auto& [ts_us, f] : frames) {
      f.timestamp = base_time + std::chrono::microseconds(ts_us - first_ts);

      if (!has_first_frame) {
        first_frame_time = f.timestamp;
        has_first_frame = true;
      }

      stats.record(f);
      if (f.error) continue;

      scrollback.push_back(f);

      // Decode signals
      if (dbc.has_message(f.id)) {
        auto decoded = dbc.decode(f);
        for (const auto& sig : decoded) {
          signals.push(signal_key{.msg_id = f.id, .name = sig.name},
                       f.timestamp, sig.value, sig.unit, sig.minimum,
                       sig.maximum);
        }
      }

      // Update monitor rows
      bool found = false;
      for (auto& row : monitor_rows) {
        if (row.frame.id == f.id && row.frame.extended == f.extended &&
            row.frame.source == f.source) {
          auto dt = f.timestamp - row.frame.timestamp;
          row.dt_ms = std::chrono::duration<float, std::milli>(dt).count();
          row.frame = f;
          row.count++;
          found = true;
          break;
        }
      }
      if (!found) {
        monitor_rows.push_back({.frame = f, .count = 1, .dt_ms = 0.f});
      }
    }

    // Trim scrollback if over limit
    if (scrollback.size() > k_max_scrollback)
      scrollback.erase(
          scrollback.begin(),
          scrollback.begin() +
              static_cast<long>(scrollback.size() - k_max_scrollback));

    return static_cast<float>(duration_sec);
  }
};

}  // namespace jcan
