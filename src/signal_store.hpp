#pragma once

#include <algorithm>
#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

namespace jcan {

struct signal_sample {
  using clock = std::chrono::steady_clock;
  clock::time_point time{};
  double value{};
};

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

struct channel_info {
  signal_key key;
  std::string unit;
  double minimum{};
  double maximum{};
  double last_value{};
  signal_sample::clock::time_point last_time{};
};

class signal_store {
 public:
  static constexpr double k_default_max_seconds = 600.0;

  void set_max_seconds(double s) { max_seconds_ = s; }
  double max_seconds() const { return max_seconds_; }

  void push(const signal_key& key, signal_sample::clock::time_point t,
            double value, const std::string& unit = {}, double minimum = 0.0,
            double maximum = 0.0) {
    auto& buf = data_[key];
    buf.push_back({t, value});

    auto& info = channels_[key];
    info.key = key;
    if (!unit.empty()) info.unit = unit;
    if (minimum != maximum) {
      info.minimum = minimum;
      info.maximum = maximum;
    }
    info.last_value = value;
    info.last_time = t;

    if (max_seconds_ > 0 && buf.size() > 2) {
      auto cutoff =
          t - std::chrono::duration_cast<signal_sample::clock::duration>(
                  std::chrono::duration<double>(max_seconds_));
      while (buf.size() > 1 && buf.front().time < cutoff) {
        buf.pop_front();
      }
    }
  }

  [[nodiscard]] const std::deque<signal_sample>* samples(
      const signal_key& key) const {
    auto it = data_.find(key);
    if (it == data_.end()) return nullptr;
    return &it->second;
  }

  [[nodiscard]] const channel_info* channel(const signal_key& key) const {
    auto it = channels_.find(key);
    if (it == channels_.end()) return nullptr;
    return &it->second;
  }

  [[nodiscard]] std::vector<const channel_info*> all_channels() const {
    std::vector<const channel_info*> out;
    out.reserve(channels_.size());
    for (const auto& [k, v] : channels_) out.push_back(&v);
    std::sort(out.begin(), out.end(), [](const auto* a, const auto* b) {
      if (a->key.msg_id != b->key.msg_id) return a->key.msg_id < b->key.msg_id;
      return a->key.name < b->key.name;
    });
    return out;
  }

  [[nodiscard]] std::size_t channel_count() const { return channels_.size(); }

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
