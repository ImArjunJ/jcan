#include <atomic>
#include <chrono>
#include <cstdio>
#include <format>
#include <iostream>
#include <stop_token>
#include <string>
#include <thread>

#include "discovery.hpp"
#include "frame_buffer.hpp"
#include "hardware.hpp"
#include "types.hpp"

static void print_frame(const jcan::can_frame& f) {
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                f.timestamp.time_since_epoch())
                .count();

  std::string data_hex;
  for (uint8_t i = 0; i < f.dlc; ++i) {
    if (i) data_hex += ' ';
    data_hex += std::format("{:02X}", f.data[i]);
  }

  std::cout << std::format("[{:>14}]  {:>{}}{:>3X}  [{}]  {}\n", us,
                           f.extended ? "" : " ", f.extended ? 8 : 3, f.id,
                           f.dlc, data_hex);
}

static void io_loop(std::stop_token stop, jcan::adapter& hw,
                    jcan::frame_buffer<>& buf) {
  while (!stop.stop_requested()) {
    auto result = adapter_recv(hw, 50);
    if (!result) {
      std::cerr << std::format("recv error: {}\n",
                               jcan::to_string(result.error()));
      continue;
    }
    if (result->has_value()) {
      buf.push(result->value());
    }
  }
}

int main() {
  std::cout << "jcan - CLI frame dump\n\n";

  auto devices = jcan::discover_adapters();
  if (devices.empty()) {
    std::cerr << "No CAN adapters found.\n";
    return 1;
  }

  std::cout << "Available adapters:\n";
  for (std::size_t i = 0; i < devices.size(); ++i) {
    std::cout << std::format("  [{}] {} - {}\n", i, devices[i].port,
                             devices[i].friendly_name);
  }

  std::cout << "\nSelect adapter index: ";
  std::size_t idx = 0;
  std::cin >> idx;
  if (idx >= devices.size()) {
    std::cerr << "Invalid selection.\n";
    return 1;
  }

  const auto& desc = devices[idx];
  auto hw = jcan::make_adapter(desc);

  std::cout << std::format("Opening {} ...\n", desc.port);
  if (auto r = jcan::adapter_open(hw, desc.port); !r) {
    std::cerr << std::format("open failed: {}\n", jcan::to_string(r.error()));
    return 1;
  }

  jcan::frame_buffer<> buf;
  std::jthread io_thread(io_loop, std::ref(hw), std::ref(buf));

  std::cout << "Listening - press Ctrl+C to stop.\n\n";
  std::cout << std::format("{:>16}  {:>8}  {:>5}  {}\n", "TIMESTAMP(us)", "ID",
                           "[DLC]", "DATA");
  std::cout << std::string(60, '-') << '\n';

  while (true) {
    auto frames = buf.drain();
    for (const auto& f : frames) print_frame(f);

    if (frames.empty())
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  (void)jcan::adapter_close(hw);
  return 0;
}
