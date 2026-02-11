#pragma once

#include <algorithm>
#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace jcan {

/// A single timestamped sample.
struct signal_sample {
  using clock = std::chrono::steady_clock;
  clock::time_point time{};
  double value{};
};

/// Unique key for a signal: message ID + signal name.
struct signal_key {
  uint32_t msg_id{};
  std::string name;

  bool operator==(const signal_key&) const = default;
};

struct signal_key_hash {
  std::size_t operator()(const signal_key& k) const noexcept {
    auto h1 = std::hash<uint32_t>{}(k.msg_id);
    auto h2 = std::hash<std::string>{}(k.name);
    return h1 ^ (h2 << 1);
  }
};

/// Metadata about a signal channel.
struct channel_info {
  signal_key key;
  std::string unit;
  double minimum{};
  double maximum{};
  double last_value{};
  signal_sample::clock::time_point last_time{};
};

/// Ring buffer of decoded signal values over time.
///
/// Call push() for every decoded signal every time poll_frames runs.
/// The store keeps up to `max_seconds` of history per signal.
class signal_store {
 public:
  static constexpr double k_default_max_seconds = 600.0;  // 10 minutes

  void set_max_seconds(double s) { max_seconds_ = s; }
  double max_seconds() const { return max_seconds_; }

  /// Push a new sample for a signal.
  void push(const signal_key& key, signal_sample::clock::time_point t,
            double value, const std::string& unit = {},
            double minimum = 0.0, double maximum = 0.0) {
    auto& buf = data_[key];
    buf.push_back({t, value});

    // Update channel info
    auto& info = channels_[key];
    info.key = key;
    if (!unit.empty()) info.unit = unit;
    if (minimum != maximum) {
      info.minimum = minimum;
      info.maximum = maximum;
    }
    info.last_value = value;
    info.last_time = t;

    // Trim old samples
    if (max_seconds_ > 0 && buf.size() > 2) {
      auto cutoff =
          t - std::chrono::duration_cast<signal_sample::clock::duration>(
                  std::chrono::duration<double>(max_seconds_));
      while (buf.size() > 1 && buf.front().time < cutoff) {
        buf.pop_front();
      }
    }
  }

  /// Get the sample buffer for a signal.
  [[nodiscard]] const std::deque<signal_sample>* samples(
      const signal_key& key) const {
    auto it = data_.find(key);
    if (it == data_.end()) return nullptr;
    return &it->second;
  }

  /// Get channel info for a signal.
  [[nodiscard]] const channel_info* channel(const signal_key& key) const {
    auto it = channels_.find(key);
    if (it == channels_.end()) return nullptr;
    return &it->second;
  }

  /// Get all known channel infos, sorted by msg_id then name.
  [[nodiscard]] std::vector<const channel_info*> all_channels() const {
    std::vector<const channel_info*> out;
    out.reserve(channels_.size());
    for (const auto& [k, v] : channels_) out.push_back(&v);
    std::sort(out.begin(), out.end(), [](const auto* a, const auto* b) {
      if (a->key.msg_id != b->key.msg_id)
        return a->key.msg_id < b->key.msg_id;
      return a->key.name < b->key.name;
    });
    return out;
  }

  /// Total number of known signal channels.
  [[nodiscard]] std::size_t channel_count() const { return channels_.size(); }

  /// Total number of samples across all channels.
  [[nodiscard]] std::size_t total_samples() const {
    std::size_t n = 0;
    for (const auto& [k, v] : data_) n += v.size();
    return n;
  }

  void clear() {
    data_.clear();
    channels_.clear();
  }

 private:
  double max_seconds_{k_default_max_seconds};
  std::unordered_map<signal_key, std::deque<signal_sample>, signal_key_hash>
      data_;
  std::unordered_map<signal_key, channel_info, signal_key_hash> channels_;
};

}  // namespace jcan
