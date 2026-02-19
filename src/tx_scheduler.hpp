#pragma once

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "frame_buffer.hpp"
#include "hardware.hpp"
#include "signal_source.hpp"
#include "types.hpp"

namespace jcan {

struct tx_job {
  uint32_t instance_id{0};
  uint32_t msg_id{0};
  std::string msg_name;
  can_frame frame{};
  float period_ms{100.f};
  bool enabled{false};
  bool is_raw{false};

  std::unordered_map<std::string, signal_source> signal_sources;

  using clock = std::chrono::steady_clock;
  clock::time_point last_sent{};
  clock::time_point start_time{};
  bool was_enabled{false};

  [[nodiscard]] double elapsed_sec() const {
    return std::chrono::duration<double>(clock::now() - start_time).count();
  }

  [[nodiscard]] std::unordered_map<std::string, double> evaluate_signals()
      const {
    std::unordered_map<std::string, double> vals;
    double t = elapsed_sec();
    for (const auto& [name, src] : signal_sources) vals[name] = src.evaluate(t);
    return vals;
  }

  static uint32_t next_id() {
    static uint32_t counter = 0;
    return ++counter;
  }
};

class tx_scheduler {
 public:
  void upsert(tx_job job) {
    std::lock_guard lk(mtx_);
    for (auto& j : jobs_) {
      if (j.instance_id == job.instance_id) {
        j = std::move(job);
        return;
      }
    }
    jobs_.push_back(std::move(job));
  }

  void remove(uint32_t instance_id) {
    std::lock_guard lk(mtx_);
    std::erase_if(
        jobs_, [&](const tx_job& j) { return j.instance_id == instance_id; });
  }

  void clear() {
    std::lock_guard lk(mtx_);
    jobs_.clear();
  }

  [[nodiscard]] std::vector<can_frame> drain_sent() {
    return sent_buf_.drain();
  }

  template <typename Fn>
  void with_jobs(Fn&& fn) {
    std::lock_guard lk(mtx_);
    fn(jobs_);
  }

  void start(adapter& hw) {
    stop();
    thread_.emplace([this, &hw](std::stop_token stop) { run(stop, hw); });
  }

  void stop() { thread_.reset(); }

  bool running() const { return thread_.has_value(); }

 private:
  void run(std::stop_token stop, adapter& hw) {
    using namespace std::chrono;

    while (!stop.stop_requested()) {
      auto now = steady_clock::now();
      float min_wait_ms = 100.f;

      {
        std::lock_guard lk(mtx_);
        for (auto& job : jobs_) {
          if (!job.enabled) continue;
          auto elapsed =
              duration<float, std::milli>(now - job.last_sent).count();
          if (elapsed >= job.period_ms) {
            if (adapter_send(hw, job.frame)) {
              can_frame logged = job.frame;
              logged.tx = true;
              logged.timestamp = can_frame::clock::now();
              sent_buf_.push(logged);
            }
            job.last_sent = now;
            min_wait_ms = std::min(min_wait_ms, job.period_ms);
          } else {
            float remaining = job.period_ms - elapsed;
            min_wait_ms = std::min(min_wait_ms, remaining);
          }
        }
      }

      if (min_wait_ms < 2.f) {
        auto deadline = now + microseconds(static_cast<int64_t>(min_wait_ms * 1000.f));
        while (steady_clock::now() < deadline) {
          std::this_thread::yield();
        }
      } else {
        std::this_thread::sleep_for(microseconds(
            static_cast<int64_t>((min_wait_ms - 1.f) * 1000.f)));
      }
    }
  }

  std::mutex mtx_;
  std::vector<tx_job> jobs_;
  std::optional<std::jthread> thread_;
  frame_buffer<4096> sent_buf_;
};

}  // namespace jcan
