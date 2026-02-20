#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
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
#include "motec_ld.hpp"
#include "permissions.hpp"
#include "signal_store.hpp"
#include "theme.hpp"
#include "tx_scheduler.hpp"
#include "types.hpp"

namespace jcan {

struct bus_stats {
  using clock = std::chrono::steady_clock;

  struct id_stats {
    uint64_t total_count{0};
    uint64_t window_count{0};
    float rate_hz{0.f};
    uint8_t last_source{0};
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
    st.last_source = f.source;
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
    std::atomic<bool> io_paused{false};
    dbc_engine slot_dbc;

    void start_io() {
      io_thread.emplace([this](std::stop_token stop) {
        while (!stop.stop_requested()) {
          if (io_paused.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
          }
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
  bool log_mode{false};
  std::string status_text{"Disconnected"};

  struct frame_row {
    can_frame frame;
    uint64_t count{1};
    float dt_ms{0.f};
    float sig_height{0.f};
  };
  std::vector<frame_row> monitor_rows;
  struct monitor_key {
    uint32_t id;
    bool extended;
    uint8_t source;
    bool operator==(const monitor_key&) const = default;
  };
  struct monitor_key_hash {
    std::size_t operator()(const monitor_key& k) const {
      return std::hash<uint64_t>{}(
          (static_cast<uint64_t>(k.id) << 9) |
          (k.extended ? 0x100ULL : 0) | k.source);
    }
  };
  std::unordered_map<monitor_key, int, monitor_key_hash> monitor_index;
  std::vector<frame_row> frozen_rows;
  std::deque<can_frame> scrollback;
  bool monitor_autoscroll{true};
  bool monitor_freeze{false};
  char filter_text[64]{};
  char scrollback_filter_text[64]{};
  static constexpr std::size_t k_max_scrollback = 100'000;

  bool show_connection{true};
  float ui_scale{1.0f};
  bool show_signals{true};
  bool show_transmitter{true};
  bool show_statistics{true};
  bool show_plotter{true};
  ImFont* mono_font{nullptr};
  semantic_colors colors{};
  theme_id current_theme{theme_id::dark_flat};

  can_frame::clock::time_point first_frame_time{};
  bool has_first_frame{false};

  std::map<uint8_t, dbc_engine> log_dbc;
  std::vector<can_frame> imported_frames;
  std::set<uint8_t> log_channels;

  tx_scheduler tx_sched;

  frame_logger logger;
  std::filesystem::path log_dir;
  std::string session_log_path;
  signal_store signals;

  struct log_layer {
    std::string name;
    std::string path;
    signal_store signals;
    bool visible{true};
    float time_offset_sec{0.0f};
    float duration_sec{0.0f};
    ImU32 tint{IM_COL32(255, 255, 255, 255)};
    signal_sample::clock::time_point base_time{};
  };
  std::vector<log_layer> overlay_layers;
  signal_sample::clock::time_point primary_base_time{};

  static constexpr ImU32 layer_tints[] = {
      IM_COL32(255, 160, 80, 255),  IM_COL32(80, 200, 255, 255),
      IM_COL32(255, 100, 200, 255), IM_COL32(180, 255, 100, 255),
      IM_COL32(200, 150, 255, 255), IM_COL32(255, 255, 100, 255),
  };

  frame_buffer<8192> replay_buf;
  std::optional<std::jthread> replay_thread;
  std::atomic<bool> replaying{false};
  std::atomic<bool> replay_paused{false};
  std::atomic<float> replay_speed{1.0f};
  std::atomic<float> replay_progress{0.f};
  std::atomic<std::size_t> replay_total_frames{0};

  bus_stats stats;

  std::optional<std::jthread> export_thread;
  std::atomic<bool> exporting{false};
  std::atomic<float> export_progress{0.f};
  std::string export_result_msg;

  static const dbc_engine& empty_dbc() {
    static const dbc_engine e;
    return e;
  }

  const dbc_engine& dbc_for_frame(const can_frame& f) const {
    if (log_mode) {
      auto it = log_dbc.find(f.source);
      if (it != log_dbc.end() && it->second.loaded()) return it->second;
      return empty_dbc();
    }
    if (f.source < adapter_slots.size()) {
      auto& slot_dbc = adapter_slots[f.source]->slot_dbc;
      if (slot_dbc.loaded()) return slot_dbc;
    }
    return empty_dbc();
  }

  bool any_dbc_has_message(const can_frame& f) const {
    return dbc_for_frame(f).has_message(f.id);
  }

  bool any_dbc_loaded() const {
    if (log_mode) {
      for (const auto& [ch, eng] : log_dbc)
        if (eng.loaded()) return true;
    }
    for (const auto& slot : adapter_slots)
      if (slot->slot_dbc.loaded()) return true;
    return false;
  }

  std::string message_name_for(uint32_t id, uint8_t source) const {
    if (log_mode) {
      auto it = log_dbc.find(source);
      if (it != log_dbc.end() && it->second.loaded())
        return it->second.message_name(id);
      return {};
    }
    if (source < adapter_slots.size()) {
      auto& slot_dbc = adapter_slots[source]->slot_dbc;
      if (slot_dbc.loaded()) return slot_dbc.message_name(id);
    }
    return {};
  }

  std::string any_message_name(uint32_t id) const {
    if (log_mode) {
      for (const auto& [ch, eng] : log_dbc) {
        if (eng.loaded()) {
          auto name = eng.message_name(id);
          if (!name.empty()) return name;
        }
      }
    }
    for (const auto& slot : adapter_slots) {
      if (slot->slot_dbc.loaded()) {
        auto name = slot->slot_dbc.message_name(id);
        if (!name.empty()) return name;
      }
    }
    return {};
  }

  std::vector<decoded_signal> any_decode(const can_frame& f) const {
    return dbc_for_frame(f).decode(f);
  }

  std::vector<uint32_t> all_message_ids() const {
    std::set<uint32_t> ids;
    for (const auto& slot : adapter_slots) {
      if (slot->slot_dbc.loaded())
        for (auto mid : slot->slot_dbc.message_ids()) ids.insert(mid);
    }
    if (log_mode) {
      for (const auto& [ch, eng] : log_dbc)
        if (eng.loaded())
          for (auto mid : eng.message_ids()) ids.insert(mid);
    }
    return {ids.begin(), ids.end()};
  }

  const dbc_engine& dbc_for_id(uint32_t id) const {
    for (const auto& slot : adapter_slots) {
      if (slot->slot_dbc.loaded() && slot->slot_dbc.has_message(id))
        return slot->slot_dbc;
    }
    if (log_mode) {
      for (const auto& [ch, eng] : log_dbc) {
        if (eng.loaded() && eng.has_message(id)) return eng;
      }
    }
    return empty_dbc();
  }

  void start_export(const std::string& path) {
    if (exporting.load()) return;
    if (session_log_path.empty() || !logger.recording()) {
      export_result_msg = "No active session log";
      return;
    }
    logger.flush();
    auto src = session_log_path;
    auto count = logger.frame_count();
    exporting.store(true);
    export_progress.store(0.f);
    export_result_msg.clear();

    export_thread.emplace([this, path, src, count](std::stop_token stop) {
      auto dst_ext = std::filesystem::path(path).extension().string();
      for (auto& c : dst_ext) c = static_cast<char>(std::tolower(c));
      auto src_ext = std::filesystem::path(src).extension().string();
      for (auto& c : src_ext) c = static_cast<char>(std::tolower(c));

      if (dst_ext == src_ext) {
        std::error_code ec;
        std::filesystem::copy_file(
            src, path, std::filesystem::copy_options::overwrite_existing, ec);
        export_progress.store(1.f);
        if (ec)
          export_result_msg = std::format("Export failed: {}", ec.message());
        else
          export_result_msg = std::format("Exported {} frames (copied)", count);
        exporting.store(false);
        return;
      }

      std::vector<std::pair<int64_t, can_frame>> frames;
      if (src_ext == ".asc")
        frames = frame_logger::load_asc(src);
      else
        frames = frame_logger::load_csv(src);

      bool asc = (dst_ext == ".asc");
      std::ofstream ofs(path, std::ios::out | std::ios::trunc);
      if (!ofs.is_open()) {
        export_result_msg = "Export failed: could not open file";
        exporting.store(false);
        return;
      }

      if (asc) {
        ofs << "date Thu Jan  1 00:00:00 AM 1970\n";
        ofs << "base hex  timestamps absolute\n";
        ofs << "internal events logged\n";
        ofs << "Begin TriggerBlock Thu Jan  1 00:00:00 AM 1970\n";
      } else {
        ofs << "timestamp_us,dir,id,extended,rtr,dlc,fd,brs,data\n";
      }

      for (std::size_t i = 0; i < frames.size(); ++i) {
        if (stop.stop_requested()) break;
        auto& [ts_us, f] = frames[i];
        uint8_t len = frame_payload_len(f);

        if (asc) {
          double seconds = static_cast<double>(ts_us) / 1e6;
          ofs << std::format("{:>12.6f}", seconds) << "  1  ";
          if (f.extended)
            ofs << std::format("{:08X}", f.id) << "x";
          else
            ofs << std::format("{:03X}", f.id);
          ofs << (f.tx ? "  Tx  " : "  Rx  ") << (f.fd ? "fd  " : "d  ")
              << static_cast<int>(len);
          for (uint8_t j = 0; j < len; ++j)
            ofs << std::format("  {:02X}", f.data[j]);
          if (f.fd && f.brs) ofs << "  BRS";
          ofs << "\n";
        } else {
          ofs << ts_us << "," << (f.tx ? "Tx" : "Rx") << ",0x"
              << std::format("{:03X}", f.id) << "," << (f.extended ? 1 : 0)
              << "," << (f.rtr ? 1 : 0) << "," << static_cast<int>(f.dlc) << ","
              << (f.fd ? 1 : 0) << "," << (f.brs ? 1 : 0) << ",";
          for (uint8_t j = 0; j < len; ++j) {
            if (j) ofs << ' ';
            ofs << std::format("{:02X}", f.data[j]);
          }
          ofs << "\n";
        }

        if ((i & 0xFFF) == 0)
          export_progress.store(static_cast<float>(i + 1) /
                                static_cast<float>(frames.size()));
      }

      if (asc) ofs << "End TriggerBlock\n";
      export_progress.store(1.f);
      export_result_msg = std::format("Exported {} frames", frames.size());
      exporting.store(false);
    });
  }

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

    for (auto& s : adapter_slots) s->io_paused.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    auto slot = std::make_unique<adapter_slot>();
    slot->desc = desc;
    slot->hw = make_adapter(desc);
    auto bitrate = static_cast<slcan_bitrate>(selected_bitrate);
    if (auto r = adapter_open(slot->hw, desc.port, bitrate); !r) {
      for (auto& s : adapter_slots) s->io_paused.store(false);
      if (r.error() == error_code::permission_denied) {
#ifdef _WIN32
        status_text = std::format(
            "Access denied: {} - device may be held by a vendor driver",
            desc.port);
        return;
#else
        status_text =
            std::format("Permission denied: {} - requesting fix...", desc.port);
        if (install_udev_rule()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1500));
          slot->hw = make_adapter(desc);
          if (auto r2 = adapter_open(slot->hw, desc.port, bitrate); !r2) {
            status_text = std::format(
                "Still failed after udev fix. Try unplugging and "
                "replugging the device, then click Connect again.");
            return;
          }
          status_text = "Permissions fixed!";
        } else {
          status_text = "Permission fix cancelled or pkexec not available.";
          return;
        }
#endif
      } else {
        status_text = std::format("Open failed: {} - {}", desc.port, to_string(r.error()));
        return;
      }
    }
    for (auto& s : adapter_slots) s->io_paused.store(false);
    slot->start_io();
    adapter_slots.push_back(std::move(slot));
    connected = true;
    log_mode = false;
    imported_frames.clear();
    log_dbc.clear();
    log_channels.clear();
    status_text =
        std::format("Connected: {} ({} adapter{})", desc.friendly_name,
                    adapter_slots.size(), adapter_slots.size() > 1 ? "s" : "");

    if (adapter_slots.size() == 1) {
      tx_slot_idx = 0;
      tx_sched.start(adapter_slots[0]->hw);
      if (!logger.recording()) {
        auto_start_session_log();
      }
    }
  }

  void auto_start_session_log() {
    std::error_code ec;
    std::filesystem::create_directories(log_dir, ec);
    if (ec) {
      status_text = std::format("Log dir error: {}", ec.message());
      return;
    }
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    auto filename = std::format("{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}.csv",
                                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                                tm.tm_hour, tm.tm_min, tm.tm_sec);
    auto path = log_dir / filename;
    session_log_path = path.string();
    logger.start_csv(path);
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

    (void)tx_sched.drain_sent();

    for (auto& f : frames) {
      if (!has_first_frame) {
        first_frame_time = f.timestamp;
        has_first_frame = true;
      }

      stats.record(f);

      if (f.error) continue;

      scrollback.push_back(f);
      while (scrollback.size() > k_max_scrollback) scrollback.pop_front();
      logger.log(f);

      if (dbc_for_frame(f).has_message(f.id)) {
        auto decoded = dbc_for_frame(f).decode(f);
        for (const auto& sig : decoded) {
          signals.push(signal_key{.msg_id = f.id, .name = sig.name},
                       f.timestamp, sig.value, sig.unit, sig.minimum,
                       sig.maximum);
        }
      }

      if (!monitor_freeze) {
        monitor_key mk{f.id, f.extended, f.source};
        auto it = monitor_index.find(mk);
        if (it != monitor_index.end()) {
          auto& row = monitor_rows[it->second];
          auto dt = f.timestamp - row.frame.timestamp;
          row.dt_ms = std::chrono::duration<float, std::milli>(dt).count();
          row.frame = f;
          row.count++;
        } else {
          monitor_index[mk] = static_cast<int>(monitor_rows.size());
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

  bool charts_dirty{false};

  void clear_monitor() {
    monitor_rows.clear();
    monitor_index.clear();
    frozen_rows.clear();
    scrollback.clear();
    stats.reset();
    signals.clear();
    overlay_layers.clear();
    has_first_frame = false;
    charts_dirty = true;
  }

  float import_log(std::vector<std::pair<int64_t, can_frame>> frames) {
    if (frames.empty()) return 0.f;
    log_mode = true;
    log_dbc.clear();
    clear_monitor();

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
    primary_base_time = base_time;

    imported_frames.clear();
    log_channels.clear();

    for (auto& [ts_us, f] : frames) {
      f.timestamp = base_time + std::chrono::microseconds(ts_us - first_ts);
      log_channels.insert(f.source);

      if (!has_first_frame) {
        first_frame_time = f.timestamp;
        has_first_frame = true;
      }

      stats.record(f);
      if (f.error) continue;

      imported_frames.push_back(f);
      scrollback.push_back(f);

      if (dbc_for_frame(f).has_message(f.id)) {
        auto decoded = dbc_for_frame(f).decode(f);
        for (const auto& sig : decoded) {
          signals.push(signal_key{.msg_id = f.id, .name = sig.name},
                       f.timestamp, sig.value, sig.unit, sig.minimum,
                       sig.maximum);
        }
      }

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

    while (scrollback.size() > k_max_scrollback) scrollback.pop_front();

    return static_cast<float>(duration_sec);
  }

  void redecode_log() {
    if (!log_mode || imported_frames.empty()) return;
    signals.clear();
    for (const auto& f : imported_frames) {
      if (dbc_for_frame(f).has_message(f.id)) {
        auto decoded = dbc_for_frame(f).decode(f);
        for (const auto& sig : decoded) {
          signals.push(signal_key{.msg_id = f.id, .name = sig.name},
                       f.timestamp, sig.value, sig.unit, sig.minimum,
                       sig.maximum);
        }
      }
    }
  }

  float import_motec(const motec::ld_file& ld) {
    if (ld.channels.empty()) return 0.f;

    log_mode = true;
    log_dbc.clear();
    clear_monitor();
    imported_frames.clear();
    log_channels.clear();

    double duration_sec = ld.duration_seconds();
    if (duration_sec < 0.1) duration_sec = 1.0;

    if (duration_sec > signals.max_seconds()) {
      signals.set_max_seconds(duration_sec * 1.1);
    }

    auto now = can_frame::clock::now();
    auto log_duration = std::chrono::duration_cast<can_frame::clock::duration>(
        std::chrono::duration<double>(duration_sec));
    auto base_time = now - log_duration;
    primary_base_time = base_time;

    first_frame_time = base_time;
    has_first_frame = true;

    constexpr uint32_t motec_msg_id = 0;

    for (const auto& ch : ld.channels) {
      if (ch.samples.empty() || ch.freq_hz == 0) continue;

      signal_key key{.msg_id = motec_msg_id, .name = ch.name};

      double ch_min = ch.samples[0];
      double ch_max = ch.samples[0];
      for (double v : ch.samples) {
        ch_min = std::min(ch_min, v);
        ch_max = std::max(ch_max, v);
      }

      double sample_period = 1.0 / static_cast<double>(ch.freq_hz);
      for (std::size_t i = 0; i < ch.samples.size(); ++i) {
        double t_sec = static_cast<double>(i) * sample_period;
        auto t =
            base_time + std::chrono::duration_cast<can_frame::clock::duration>(
                            std::chrono::duration<double>(t_sec));
        signals.push(key, t, ch.samples[i], ch.unit, ch_min, ch_max);
      }
    }

    return static_cast<float>(duration_sec);
  }

  float import_overlay_log(std::vector<std::pair<int64_t, can_frame>> frames,
                           const std::string& filepath) {
    if (frames.empty()) return 0.f;

    int64_t first_ts = frames.front().first;
    int64_t last_ts = frames.back().first;
    double duration_sec = static_cast<double>(last_ts - first_ts) / 1e6;
    if (duration_sec < 0.1) duration_sec = 1.0;

    log_layer layer;
    layer.name = std::filesystem::path(filepath).stem().string();
    layer.path = filepath;
    layer.duration_sec = static_cast<float>(duration_sec);
    layer.tint = layer_tints[overlay_layers.size() %
                             (sizeof(layer_tints) / sizeof(layer_tints[0]))];
    layer.base_time = primary_base_time;
    if (duration_sec > layer.signals.max_seconds())
      layer.signals.set_max_seconds(duration_sec * 1.1);

    for (auto& [ts_us, f] : frames) {
      f.timestamp = primary_base_time + std::chrono::microseconds(ts_us - first_ts);
      if (f.error) continue;
      if (dbc_for_frame(f).has_message(f.id)) {
        auto decoded = dbc_for_frame(f).decode(f);
        for (const auto& sig : decoded)
          layer.signals.push(signal_key{.msg_id = f.id, .name = sig.name},
                             f.timestamp, sig.value, sig.unit, sig.minimum,
                             sig.maximum);
      }
    }

    auto dur = static_cast<float>(duration_sec);
    overlay_layers.push_back(std::move(layer));
    return dur;
  }

  float import_overlay_motec(const motec::ld_file& ld,
                             const std::string& filepath) {
    if (ld.channels.empty()) return 0.f;

    double duration_sec = ld.duration_seconds();
    if (duration_sec < 0.1) duration_sec = 1.0;

    log_layer layer;
    layer.name = std::filesystem::path(filepath).stem().string();
    layer.path = filepath;
    layer.duration_sec = static_cast<float>(duration_sec);
    layer.tint = layer_tints[overlay_layers.size() %
                             (sizeof(layer_tints) / sizeof(layer_tints[0]))];
    layer.base_time = primary_base_time;
    if (duration_sec > layer.signals.max_seconds())
      layer.signals.set_max_seconds(duration_sec * 1.1);

    constexpr uint32_t motec_msg_id = 0;
    for (const auto& ch : ld.channels) {
      if (ch.samples.empty() || ch.freq_hz == 0) continue;
      signal_key key{.msg_id = motec_msg_id, .name = ch.name};
      double ch_min = ch.samples[0], ch_max = ch.samples[0];
      for (double v : ch.samples) {
        ch_min = std::min(ch_min, v);
        ch_max = std::max(ch_max, v);
      }
      double sample_period = 1.0 / static_cast<double>(ch.freq_hz);
      for (std::size_t i = 0; i < ch.samples.size(); ++i) {
        double t_sec = static_cast<double>(i) * sample_period;
        auto t = primary_base_time + std::chrono::duration_cast<can_frame::clock::duration>(
                                         std::chrono::duration<double>(t_sec));
        layer.signals.push(key, t, ch.samples[i], ch.unit, ch_min, ch_max);
      }
    }

    overlay_layers.push_back(std::move(layer));
    return static_cast<float>(duration_sec);
  }

  void remove_overlay(int index) {
    if (index >= 0 && index < static_cast<int>(overlay_layers.size()))
      overlay_layers.erase(overlay_layers.begin() + index);
  }
};

}  // namespace jcan
