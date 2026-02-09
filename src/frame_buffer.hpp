#pragma once

#include <array>
#include <cstddef>
#include <mutex>
#include <optional>
#include <vector>

#include "types.hpp"

namespace jcan {

template <std::size_t capacity = 4096>
class frame_buffer {
 public:
  bool push(const can_frame& frame) {
    std::lock_guard lock(mtx_);
    buf_[head_] = frame;
    head_ = (head_ + 1) % capacity;
    if (count_ < capacity) {
      ++count_;
      return true;
    }
    tail_ = (tail_ + 1) % capacity;
    return false;
  }

  [[nodiscard]] std::optional<can_frame> pop() {
    std::lock_guard lock(mtx_);
    if (count_ == 0) return std::nullopt;
    auto& f = buf_[tail_];
    tail_ = (tail_ + 1) % capacity;
    --count_;
    return f;
  }

  [[nodiscard]] std::vector<can_frame> drain() {
    std::lock_guard lock(mtx_);
    std::vector<can_frame> out;
    out.reserve(count_);
    while (count_ > 0) {
      out.push_back(buf_[tail_]);
      tail_ = (tail_ + 1) % capacity;
      --count_;
    }
    return out;
  }

  [[nodiscard]] std::size_t size() const {
    std::lock_guard lock(mtx_);
    return count_;
  }

  [[nodiscard]] bool empty() const {
    std::lock_guard lock(mtx_);
    return count_ == 0;
  }

  void clear() {
    std::lock_guard lock(mtx_);
    head_ = tail_ = count_ = 0;
  }

 private:
  mutable std::mutex mtx_;
  std::array<can_frame, capacity> buf_{};
  std::size_t head_{0};
  std::size_t tail_{0};
  std::size_t count_{0};
};

}  // namespace jcan
