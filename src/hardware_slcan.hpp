#pragma once

#include <libserialport.h>

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <string_view>

#include "types.hpp"

namespace jcan {

struct serial_slcan {
  struct sp_port* port_{nullptr};
  bool open_{false};
  std::string rx_accum_;

  static constexpr unsigned k_default_timeout_ms = 100;

  [[nodiscard]] result<> open(const std::string& port_path,
                              slcan_bitrate bitrate = slcan_bitrate::s6,
                              unsigned baud = 115200) {
    if (open_) return std::unexpected(error_code::already_open);

    if (sp_get_port_by_name(port_path.c_str(), &port_) != SP_OK)
      return std::unexpected(error_code::port_not_found);

    if (sp_open(port_, SP_MODE_READ_WRITE) != SP_OK) {
      auto saved_errno = errno;
      sp_free_port(port_);
      port_ = nullptr;
      if (saved_errno == EACCES || saved_errno == EPERM)
        return std::unexpected(error_code::permission_denied);
      return std::unexpected(error_code::port_open_failed);
    }

    sp_set_baudrate(port_, static_cast<int>(baud));
    sp_set_bits(port_, 8);
    sp_set_parity(port_, SP_PARITY_NONE);
    sp_set_stopbits(port_, 1);
    sp_set_flowcontrol(port_, SP_FLOWCONTROL_NONE);

    open_ = true;
    (void)send_command("C\r");

    auto br_cmd = std::format("S{}\r", static_cast<int>(bitrate));
    if (auto r = send_command(br_cmd); !r) return r;
    if (auto r = send_command("O\r"); !r) return r;

    return {};
  }

  [[nodiscard]] result<> close() {
    if (!open_) return std::unexpected(error_code::not_open);
    (void)send_command("C\r");
    sp_close(port_);
    sp_free_port(port_);
    port_ = nullptr;
    open_ = false;
    return {};
  }

  [[nodiscard]] result<> send(const can_frame& frame) {
    if (!open_) return std::unexpected(error_code::not_open);

    std::string pkt;
    uint8_t payload_len =
        std::min(frame_payload_len(frame), static_cast<uint8_t>(8));
    if (frame.extended) {
      pkt = std::format("T{:08X}{}", frame.id, payload_len);
    } else {
      pkt = std::format("t{:03X}{}", frame.id & 0x7FF, payload_len);
    }
    for (uint8_t i = 0; i < payload_len; ++i) {
      pkt += std::format("{:02X}", frame.data[i]);
    }
    pkt += '\r';

    return send_command(pkt);
  }

  [[nodiscard]] result<std::vector<can_frame>> recv_many(
      unsigned timeout_ms = k_default_timeout_ms) {
    if (!open_) return std::unexpected(error_code::not_open);

    char buf[4096]{};
    int n = sp_blocking_read(port_, buf, sizeof(buf) - 1, timeout_ms);
    if (n < 0) return std::unexpected(error_code::read_error);

    std::vector<can_frame> frames;
    if (n == 0) return frames;

    if (std::getenv("JCAN_DEBUG")) {
      std::fprintf(stderr, "[slcan] read %d bytes:", n);
      for (int i = 0; i < std::min(n, 80); ++i)
        std::fprintf(stderr, " %02X", static_cast<uint8_t>(buf[i]));
      std::fprintf(stderr, " | ");
      for (int i = 0; i < std::min(n, 80); ++i) {
        char c = buf[i];
        std::fprintf(stderr, "%c", (c >= 0x20 && c < 0x7F) ? c : '.');
      }
      std::fprintf(stderr, "\n");
    }

    rx_accum_.append(buf, static_cast<size_t>(n));

    std::size_t start = 0;
    while (true) {
      auto cr = rx_accum_.find('\r', start);
      if (cr == std::string::npos) break;

      std::string_view line(rx_accum_.data() + start, cr - start);

      auto cmd_pos = line.find_first_of("tTrRF");
      if (cmd_pos != std::string_view::npos) {
        line = line.substr(cmd_pos);
        auto parsed = parse_slcan(line);
        if (parsed && parsed->has_value()) {
          frames.push_back(parsed->value());
          if (std::getenv("JCAN_DEBUG"))
            std::fprintf(stderr, "[slcan] frame: id=0x%X dlc=%u\n",
                         parsed->value().id, parsed->value().dlc);
        } else if (std::getenv("JCAN_DEBUG")) {
          std::fprintf(stderr, "[slcan] parse fail: '%.*s'\n",
                       static_cast<int>(line.size()), line.data());
        }
      } else if (std::getenv("JCAN_DEBUG") && !line.empty()) {
        std::fprintf(stderr, "[slcan] non-frame data: '%.*s' (",
                     static_cast<int>(line.size()), line.data());
        for (std::size_t i = 0; i < line.size(); ++i)
          std::fprintf(stderr, "%02X ", static_cast<uint8_t>(line[i]));
        std::fprintf(stderr, ")\n");
      }
      start = cr + 1;
    }

    if (start > 0) rx_accum_.erase(0, start);

    if (rx_accum_.size() > 256 && rx_accum_.find('\r') == std::string::npos) {
      if (std::getenv("JCAN_DEBUG"))
        std::fprintf(stderr,
                     "[slcan] flushing %zu bytes of junk from rx_accum\n",
                     rx_accum_.size());
      rx_accum_.clear();
    }
    if (rx_accum_.size() > 8192) rx_accum_.clear();

    return frames;
  }

  [[nodiscard]] result<std::optional<can_frame>> recv(
      unsigned timeout_ms = k_default_timeout_ms) {
    auto batch = recv_many(timeout_ms);
    if (!batch) return std::unexpected(batch.error());
    if (batch->empty()) return std::optional<can_frame>{std::nullopt};
    return std::optional<can_frame>{batch->front()};
  }

  [[nodiscard]] static result<std::optional<can_frame>> parse_slcan(
      std::string_view line) {
    if (line.empty()) return std::optional<can_frame>{std::nullopt};

    can_frame f{};
    f.timestamp = can_frame::clock::now();

    char type = line[0];
    size_t id_len = 0;

    switch (type) {
      case 't':
        id_len = 3;
        f.extended = false;
        f.rtr = false;
        break;
      case 'T':
        id_len = 8;
        f.extended = true;
        f.rtr = false;
        break;
      case 'r':
        id_len = 3;
        f.extended = false;
        f.rtr = true;
        break;
      case 'R':
        id_len = 8;
        f.extended = true;
        f.rtr = true;
        break;
      case 'F': {
        if (line.size() >= 3) {
          f.error = true;
          f.dlc = 1;
          auto hex = line.substr(1, 2);
          auto [p, e] =
              std::from_chars(hex.data(), hex.data() + 2, f.data[0], 16);
          (void)p;
          if (e != std::errc{}) f.data[0] = 0xFF;
          return std::optional<can_frame>{f};
        }
        return std::optional<can_frame>{std::nullopt};
      }
      default:
        return std::optional<can_frame>{std::nullopt};
    }

    if (line.size() < 1 + id_len + 1)
      return std::unexpected(error_code::frame_parse_error);

    auto id_sv = line.substr(1, id_len);
    auto [ptr, ec] =
        std::from_chars(id_sv.data(), id_sv.data() + id_sv.size(), f.id, 16);
    if (ec != std::errc{})
      return std::unexpected(error_code::frame_parse_error);

    size_t dlc_pos = 1 + id_len;
    if (line[dlc_pos] < '0' || line[dlc_pos] > '8')
      return std::unexpected(error_code::frame_parse_error);
    f.dlc = static_cast<uint8_t>(line[dlc_pos] - '0');
    uint8_t payload_len = frame_payload_len(f);

    size_t data_start = dlc_pos + 1;
    for (uint8_t i = 0; i < payload_len; ++i) {
      size_t off = data_start + i * 2;
      if (off + 2 > line.size())
        return std::unexpected(error_code::frame_parse_error);
      auto byte_sv = line.substr(off, 2);
      auto [p2, e2] =
          std::from_chars(byte_sv.data(), byte_sv.data() + 2, f.data[i], 16);
      if (e2 != std::errc{})
        return std::unexpected(error_code::frame_parse_error);
    }

    return std::optional<can_frame>{f};
  }

 private:
  [[nodiscard]] result<> send_command(const std::string& cmd) {
    int written =
        sp_blocking_write(port_, cmd.c_str(), cmd.size(), k_default_timeout_ms);
    if (written < 0 || static_cast<size_t>(written) != cmd.size())
      return std::unexpected(error_code::write_error);
    return {};
  }
};

}  // namespace jcan
