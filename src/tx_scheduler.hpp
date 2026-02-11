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

#include "hardware.hpp"
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

  std::unordered_map<std::string, double> signal_values;

  using clock = std::chrono::steady_clock;
  clock::time_point last_sent{};

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

      {
        std::lock_guard lk(mtx_);
        for (auto& job : jobs_) {
          if (!job.enabled) continue;
          auto elapsed =
              duration<float, std::milli>(now - job.last_sent).count();
          if (elapsed >= job.period_ms) {
            (void)adapter_send(hw, job.frame);
            job.last_sent = now;
          }
        }
      }

      std::this_thread::sleep_for(500us);
    }
  }

  std::mutex mtx_;
  std::vector<tx_job> jobs_;
  std::optional<std::jthread> thread_;
};

}  // namespace jcan
