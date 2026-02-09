#pragma once

#include <libusb-1.0/libusb.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <format>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "firmware_blobs.hpp"
#include "types.hpp"

namespace jcan {

namespace vector {

inline constexpr uint16_t k_vid = 0x1248;
inline constexpr uint16_t k_pid_vn1640a = 0x1073;

inline constexpr uint8_t k_ep_cmd_out = 0x01;
inline constexpr uint8_t k_ep_cmd_resp_in = 0x82;
inline constexpr uint8_t k_ep_tx_data_out = 0x03;
inline constexpr uint8_t k_ep_tx_flow_ctrl_in = 0x84;
inline constexpr uint8_t k_ep_rx_data_in = 0x85;

inline constexpr unsigned k_cmd_timeout_ms = 2000;
inline constexpr unsigned k_rx_timeout_ms = 50;
inline constexpr unsigned k_tx_timeout_ms = 500;

inline constexpr uint32_t CMD_GET_BOOTCODE_INFO = 0x10041;
inline constexpr uint32_t CMD_GET_FIRMWARE_INFO = 0x20002;
inline constexpr uint32_t CMD_START_FIRMWARE_OP = 0x2002B;
inline constexpr uint32_t CMD_DRIVER_INITIALIZED = 0x2002C;
inline constexpr uint32_t CMD_ENABLE_DEBUG_EVENT = 0x20107;

inline constexpr uint32_t CMD_DOWNLOAD_FW_CHUNK = 0x10040;
inline constexpr uint32_t CMD_DOWNLOAD_FPGA_CHUNK = 0x20001;

inline constexpr uint32_t FW_CHUNK_MAX_DATA = 972;

inline constexpr uint32_t CMD_SET_RX_EVT_TRANSFER_MODE = 0x20007;

inline constexpr uint32_t CMD_ACTIVATE_CHANNEL = 0x30001;
inline constexpr uint32_t CMD_DEACTIVATE_CHANNEL = 0x30002;
inline constexpr uint32_t CMD_SET_OUTPUT_MODE = 0x30003;
inline constexpr uint32_t CMD_SET_CHIP_PARAM_FD_XL = 0x30010;
inline constexpr uint32_t CMD_READ_CORE_FREQUENCY = 0x30013;
inline constexpr uint32_t CMD_READ_CANXL_CAPS = 0x30015;
inline constexpr uint32_t CMD_READ_DEFAULT_CONFIG = 0x30016;

inline constexpr uint32_t CMD_SET_TRANSCEIVER_MODE = 0x1E0001;
inline constexpr uint32_t CMD_GET_TRANSCEIVER_INFO = 0x2002E;

inline constexpr uint32_t OUTPUT_MODE_NORMAL = 1;
inline constexpr uint32_t OUTPUT_MODE_SILENT = 2;
inline constexpr uint32_t OUTPUT_MODE_RESTRICTED = 4;

inline constexpr uint32_t TRANSCEIVER_MODE_NORMAL = 0x09;

inline constexpr uint16_t FW_CANFD_RX_OK = 0x0400;
inline constexpr uint16_t FW_CANFD_RX_NAK = 0x0401;
inline constexpr uint16_t FW_CANFD_RX_ERROR = 0x0402;
inline constexpr uint16_t FW_CANFD_TX_RECEIPT = 0x0403;
inline constexpr uint16_t FW_CANFD_TX_OK = 0x0404;
inline constexpr uint16_t FW_CANFD_TX_NAK = 0x0405;
inline constexpr uint16_t FW_CANFD_TX_ERROR = 0x0406;
inline constexpr uint16_t FW_CANFD_BUS_STATISTIC = 0x0407;
inline constexpr uint16_t FW_CANFD_ERR_COUNTER = 0x0408;
inline constexpr uint16_t FW_CANFD_TX_OK_ERRFR = 0x040A;
inline constexpr uint16_t FW_CANFD_TX_REMOVED = 0x040B;

inline constexpr uint16_t FW_CANXL_RX_OK = 0x0A00;

inline constexpr uint16_t XL_TIMER_EVENT = 0x0008;
inline constexpr uint16_t XL_SYNC_PULSE = 0x000B;

}  // namespace vector

struct can_bit_timing {
  uint32_t bitrate_bps;
  uint32_t tseg1;
  uint32_t tseg2;
  uint32_t sjw;
};

inline can_bit_timing compute_can_timing(uint32_t clock_hz,
                                         uint32_t bitrate_bps) {
  for (uint32_t tq = 80; tq >= 8; --tq) {
    uint32_t product = bitrate_bps * tq;
    if (product == 0 || clock_hz % product != 0) continue;
    uint32_t brp = clock_hz / product;
    if (brp < 1 || brp > 1024) continue;

    uint32_t tseg1 = (tq * 80 / 100) - 1;
    uint32_t tseg2 = tq - 1 - tseg1;
    if (tseg1 < 1 || tseg2 < 1) continue;

    uint32_t sjw = std::min(tseg2, 16u);
    return {bitrate_bps, tseg1, tseg2, sjw};
  }

  return {bitrate_bps, 63, 16, 16};
}

struct vector_xl {
  libusb_context* ctx_{nullptr};
  libusb_device_handle* dev_{nullptr};
  bool open_{false};
  uint8_t channel_{0};
  uint32_t core_clock_hz_{0};

  std::vector<uint8_t> rx_partial_;
  uint16_t rx_partial_expected_{0};

  static bool debug() { return std::getenv("JCAN_DEBUG") != nullptr; }

  [[nodiscard]] result<> open(const std::string& port,
                              slcan_bitrate bitrate = slcan_bitrate::s6,
                              [[maybe_unused]] unsigned baud = 0) {
    if (open_) return std::unexpected(error_code::already_open);

    if (auto pos = port.find(':'); pos != std::string::npos) {
      auto ch_str = port.substr(pos + 1);
      channel_ = static_cast<uint8_t>(std::atoi(ch_str.c_str()));
    } else {
      channel_ = 0;
    }

    int r = libusb_init(&ctx_);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[vector] libusb_init failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
      return std::unexpected(error_code::port_open_failed);
    }

    dev_ = libusb_open_device_with_vid_pid(ctx_, vector::k_vid,
                                           vector::k_pid_vn1640a);
    if (!dev_) {
      if (debug())
        std::fprintf(stderr, "[vector] device not found (1248:1073)\n");
      libusb_exit(ctx_);
      ctx_ = nullptr;
      return std::unexpected(error_code::port_not_found);
    }

    for (int iface = 0; iface < 2; ++iface) {
      if (libusb_kernel_driver_active(dev_, iface) == 1) {
        libusb_detach_kernel_driver(dev_, iface);
      }
    }

    r = libusb_reset_device(dev_);
    if (r < 0 && r != LIBUSB_ERROR_NOT_FOUND) {
      if (debug())
        std::fprintf(stderr, "[vector] USB reset failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
    }

    r = libusb_set_configuration(dev_, 1);
    if (r < 0 && r != LIBUSB_ERROR_BUSY) {
      if (debug())
        std::fprintf(stderr, "[vector] set_configuration failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
    }

    r = libusb_claim_interface(dev_, 0);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[vector] claim interface 0 failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
      libusb_close(dev_);
      libusb_exit(ctx_);
      dev_ = nullptr;
      ctx_ = nullptr;
      return std::unexpected(error_code::permission_denied);
    }

    {
      std::array<uint8_t, 1024> flush_buf{};
      int flush_xfer = 0;
      for (int i = 0; i < 8; ++i) {
        int fr = libusb_bulk_transfer(
            dev_, vector::k_ep_cmd_resp_in, flush_buf.data(),
            static_cast<int>(flush_buf.size()), &flush_xfer, 50);
        if (fr == LIBUSB_ERROR_TIMEOUT || flush_xfer == 0) break;
        if (debug())
          std::fprintf(stderr, "[vector] flushed %d stale bytes from EP\n",
                       flush_xfer);
      }
    }

    if (auto res = run_init_sequence(bitrate); !res) {
      libusb_release_interface(dev_, 0);
      libusb_close(dev_);
      libusb_exit(ctx_);
      dev_ = nullptr;
      ctx_ = nullptr;
      return res;
    }

    open_ = true;
    if (debug())
      std::fprintf(stderr, "[vector] VN1640A opened, channel %u\n", channel_);
    return {};
  }

  [[nodiscard]] result<> close() {
    if (!open_) return std::unexpected(error_code::not_open);

    (void)cmd_deactivate_channel(channel_);

    libusb_release_interface(dev_, 0);
    libusb_close(dev_);
    libusb_exit(ctx_);
    dev_ = nullptr;
    ctx_ = nullptr;
    open_ = false;
    rx_partial_.clear();
    if (debug()) std::fprintf(stderr, "[vector] closed\n");
    return {};
  }

  [[nodiscard]] result<> send(const can_frame& frame) {
    if (!open_) return std::unexpected(error_code::not_open);

    uint8_t payload_len = frame_payload_len(frame);

    uint32_t total_size = (static_cast<uint32_t>(payload_len) + 31u) & ~3u;
    std::array<uint8_t, 128> buf{};

    auto put_le32 = [&](size_t off, uint32_t v) {
      buf[off + 0] = static_cast<uint8_t>(v);
      buf[off + 1] = static_cast<uint8_t>(v >> 8);
      buf[off + 2] = static_cast<uint8_t>(v >> 16);
      buf[off + 3] = static_cast<uint8_t>(v >> 24);
    };
    auto put_le16 = [&](size_t off, uint16_t v) {
      buf[off + 0] = static_cast<uint8_t>(v);
      buf[off + 1] = static_cast<uint8_t>(v >> 8);
    };

    put_le32(0, total_size);
    put_le16(4, 0);
    put_le16(6, 0x0440);
    uint32_t uhandle = (static_cast<uint32_t>(channel_) << 24);
    put_le32(8, uhandle);

    uint32_t msg_ctrl = frame.dlc & 0x0F;
    if (frame.extended) msg_ctrl |= 0x20;
    if (frame.fd && frame.brs) msg_ctrl |= 0x80;
    if (frame.fd) msg_ctrl |= 0x4000;
    if (frame.rtr) msg_ctrl |= 0x10;
    put_le32(12, msg_ctrl);

    put_le32(16, frame.id);
    buf[20] = 0;

    std::memcpy(&buf[24], frame.data.data(), payload_len);

    int transferred = 0;
    int r = libusb_bulk_transfer(dev_, vector::k_ep_tx_data_out, buf.data(),
                                 static_cast<int>(total_size), &transferred,
                                 vector::k_tx_timeout_ms);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[vector] TX failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
      return std::unexpected(error_code::write_error);
    }
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
      unsigned timeout_ms = 100) {
    if (!open_) return std::unexpected(error_code::not_open);

    std::vector<can_frame> frames;
    std::array<uint8_t, 16384> buf{};

    int transferred = 0;
    int r = libusb_bulk_transfer(dev_, vector::k_ep_rx_data_in, buf.data(),
                                 static_cast<int>(buf.size()), &transferred,
                                 static_cast<unsigned>(timeout_ms));

    if (r == LIBUSB_ERROR_TIMEOUT) return frames;
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[vector] RX failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
      return std::unexpected(error_code::read_error);
    }

    if (debug() && transferred > 0) {
      std::fprintf(stderr, "[vector] RX %d bytes:", transferred);
      for (int i = 0; i < std::min(transferred, 64); ++i)
        std::fprintf(stderr, " %02X", buf[static_cast<size_t>(i)]);
      if (transferred > 64) std::fprintf(stderr, " ...");
      std::fprintf(stderr, "\n");
    }

    size_t pos = 0;
    size_t total = static_cast<size_t>(transferred);

    if (!rx_partial_.empty()) {
      size_t need =
          static_cast<size_t>(rx_partial_expected_) - rx_partial_.size();
      size_t avail = std::min(need, total);
      rx_partial_.insert(rx_partial_.end(), buf.begin(),
                         buf.begin() + static_cast<ptrdiff_t>(avail));
      pos += avail;

      if (rx_partial_.size() >= rx_partial_expected_) {
        parse_rx_event(rx_partial_.data(), rx_partial_expected_, frames);
        rx_partial_.clear();
        rx_partial_expected_ = 0;
      } else {
        return frames;
      }
    }

    while (pos + 4 <= total) {
      uint16_t evt_size = static_cast<uint16_t>(buf[pos]) |
                          (static_cast<uint16_t>(buf[pos + 1]) << 8);
      uint16_t evt_tag = static_cast<uint16_t>(buf[pos + 2]) |
                         (static_cast<uint16_t>(buf[pos + 3]) << 8);

      if (evt_size < 4 || evt_size > 4164 || evt_tag == 0 ||
          (evt_size & 3) != 0) {
        if (debug())
          std::fprintf(stderr,
                       "[vector] bad RX event: size=%u tag=0x%04X at pos=%zu\n",
                       evt_size, evt_tag, pos);
        break;
      }

      if (pos + evt_size > total) {
        rx_partial_.assign(buf.begin() + static_cast<ptrdiff_t>(pos),
                           buf.begin() + static_cast<ptrdiff_t>(total));
        rx_partial_expected_ = evt_size;
        break;
      }

      parse_rx_event(&buf[pos], evt_size, frames);
      pos += evt_size;
    }

    return frames;
  }

 private:
  static uint32_t get_le32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
  }
  static uint16_t get_le16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
  }
  static void set_le32(uint8_t* p, uint32_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
    p[2] = static_cast<uint8_t>(v >> 16);
    p[3] = static_cast<uint8_t>(v >> 24);
  }

  [[nodiscard]] result<> send_sync_cmd(uint8_t* buf, uint32_t size) {
    uint32_t aligned = (size + 3u) & ~3u;
    int transferred = 0;

    if (debug()) {
      std::fprintf(stderr,
                   "[vector] TX cmd 0x%05X (%u bytes): ", get_le32(buf + 4),
                   aligned);
      for (uint32_t i = 0; i < std::min(aligned, 32u); ++i)
        std::fprintf(stderr, "%02X ", buf[i]);
      std::fprintf(stderr, "\n");
    }

    int r = libusb_bulk_transfer(dev_, vector::k_ep_cmd_out, buf,
                                 static_cast<int>(aligned), &transferred,
                                 vector::k_cmd_timeout_ms);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[vector] cmd write failed: %s (cmd=0x%X)\n",
                     libusb_strerror(static_cast<libusb_error>(r)),
                     get_le32(buf + 4));
      return std::unexpected(error_code::write_error);
    }

    transferred = 0;
    r = libusb_bulk_transfer(dev_, vector::k_ep_cmd_resp_in, buf,
                             static_cast<int>(aligned), &transferred,
                             vector::k_cmd_timeout_ms);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[vector] cmd read failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
      return std::unexpected(error_code::read_error);
    }

    if (debug()) {
      std::fprintf(stderr,
                   "[vector] cmd 0x%05X → resp %d bytes: ", get_le32(buf + 4),
                   transferred);
      for (int i = 0; i < std::min(transferred, 32); ++i)
        std::fprintf(stderr, "%02X ", buf[i]);
      std::fprintf(stderr, "\n");
    }

    if (transferred >= 16) {
      uint32_t cmd_result = get_le32(buf + 12);
      if (cmd_result != 0) {
        if (debug())
          std::fprintf(stderr, "[vector] cmd result=0x%X (error)\n",
                       cmd_result);
        return std::unexpected(error_code::write_error);
      }
    }
    return {};
  }

  [[nodiscard]] result<> download_firmware_blob(uint32_t cmd_id,
                                                const uint8_t* data,
                                                size_t total_size) {
    constexpr size_t max_chunk = vector::FW_CHUNK_MAX_DATA;
    size_t offset = 0;
    size_t chunk_count = 0;

    while (offset < total_size) {
      size_t remaining = total_size - offset;
      auto chunk_len = static_cast<uint32_t>(std::min(remaining, max_chunk));
      bool is_last = (offset + chunk_len >= total_size);

      uint32_t cmd_size = (28u + chunk_len + 3u) & ~3u;
      std::array<uint8_t, 1008> buf{};
      set_le32(buf.data() + 0, cmd_size);
      set_le32(buf.data() + 4, cmd_id);

      set_le32(buf.data() + 16, is_last ? 1u : 0u);
      set_le32(buf.data() + 20, static_cast<uint32_t>(offset));
      set_le32(buf.data() + 24, chunk_len);
      std::memcpy(buf.data() + 28, data + offset, chunk_len);

      int transferred = 0;
      int r = libusb_bulk_transfer(dev_, vector::k_ep_cmd_out, buf.data(),
                                   static_cast<int>(cmd_size), &transferred,
                                   vector::k_cmd_timeout_ms);
      if (r < 0) {
        if (debug())
          std::fprintf(stderr,
                       "[vector] chunk write failed at offset %zu: %s\n",
                       offset, libusb_strerror(static_cast<libusb_error>(r)));
        return std::unexpected(error_code::write_error);
      }

      transferred = 0;
      r = libusb_bulk_transfer(dev_, vector::k_ep_cmd_resp_in, buf.data(),
                               static_cast<int>(cmd_size), &transferred,
                               vector::k_cmd_timeout_ms);
      if (r < 0) {
        if (debug())
          std::fprintf(stderr, "[vector] chunk read failed at offset %zu: %s\n",
                       offset, libusb_strerror(static_cast<libusb_error>(r)));
        return std::unexpected(error_code::read_error);
      }

      if (transferred >= 16) {
        uint32_t result = get_le32(buf.data() + 12);
        if (result != 0) {
          if (debug())
            std::fprintf(stderr, "[vector] chunk result=0x%X at offset %zu\n",
                         result, offset);
          return std::unexpected(error_code::write_error);
        }
      }

      offset += chunk_len;
      ++chunk_count;

      if (debug() && (chunk_count % 100 == 0 || is_last))
        std::fprintf(stderr,
                     "[vector]   progress: %zu / %zu bytes (%zu chunks)\n",
                     offset, total_size, chunk_count);
    }

    return {};
  }

  [[nodiscard]] result<> cmd_get_bootcode_info() {
    std::array<uint8_t, 136> buf{};
    set_le32(buf.data(), 136);
    set_le32(buf.data() + 4, vector::CMD_GET_BOOTCODE_INFO);
    return send_sync_cmd(buf.data(), 136);
  }

  [[nodiscard]] result<> cmd_start_firmware_op() {
    std::array<uint8_t, 16> buf{};
    set_le32(buf.data(), 16);
    set_le32(buf.data() + 4, vector::CMD_START_FIRMWARE_OP);
    return send_sync_cmd(buf.data(), 16);
  }

  [[nodiscard]] result<> cmd_driver_initialized() {
    std::array<uint8_t, 16> buf{};
    set_le32(buf.data(), 16);
    set_le32(buf.data() + 4, vector::CMD_DRIVER_INITIALIZED);
    return send_sync_cmd(buf.data(), 16);
  }

  [[nodiscard]] result<> cmd_get_firmware_info() {
    std::array<uint8_t, 156> buf{};
    set_le32(buf.data(), 156);
    set_le32(buf.data() + 4, vector::CMD_GET_FIRMWARE_INFO);
    auto r = send_sync_cmd(buf.data(), 156);
    if (!r) return r;

    if (debug()) {
      std::fprintf(stderr, "[vector] firmware info received (%u bytes)\n",
                   get_le32(buf.data()));
    }
    return {};
  }

  [[nodiscard]] result<> cmd_set_rx_evt_transfer_mode(uint32_t mode,
                                                      uint32_t cycle_time) {
    std::array<uint8_t, 24> buf{};
    set_le32(buf.data(), 24);
    set_le32(buf.data() + 4, vector::CMD_SET_RX_EVT_TRANSFER_MODE);

    uint32_t actual_mode = cycle_time ? mode : 1;
    set_le32(buf.data() + 16, actual_mode);
    set_le32(buf.data() + 20, cycle_time);
    return send_sync_cmd(buf.data(), 24);
  }

  [[nodiscard]] result<> cmd_read_core_frequency() {
    std::array<uint8_t, 144> buf{};
    set_le32(buf.data(), 144);
    set_le32(buf.data() + 4, vector::CMD_READ_CORE_FREQUENCY);
    auto r = send_sync_cmd(buf.data(), 144);
    if (!r) return r;

    uint32_t freq = get_le32(buf.data() + 16);
    if (freq != 0) {
      core_clock_hz_ = freq;
      if (debug()) std::fprintf(stderr, "[vector] core clock = %u Hz\n", freq);
    }
    return {};
  }

  [[nodiscard]] result<> cmd_read_canxl_caps() {
    std::array<uint8_t, 24> buf{};
    set_le32(buf.data(), 24);
    set_le32(buf.data() + 4, vector::CMD_READ_CANXL_CAPS);
    return send_sync_cmd(buf.data(), 24);
  }

  [[nodiscard]] result<> cmd_read_default_config(uint8_t channel) {
    std::array<uint8_t, 104> buf{};
    set_le32(buf.data(), 104);
    set_le32(buf.data() + 4, vector::CMD_READ_DEFAULT_CONFIG);

    buf[17] = channel;
    return send_sync_cmd(buf.data(), 104);
  }

  [[nodiscard]] result<> cmd_set_output_mode(uint8_t channel, uint32_t mode) {
    std::array<uint8_t, 28> buf{};
    set_le32(buf.data(), 28);
    set_le32(buf.data() + 4, vector::CMD_SET_OUTPUT_MODE);

    set_le32(buf.data() + 16, channel);

    set_le32(buf.data() + 20, mode);
    return send_sync_cmd(buf.data(), 28);
  }

  [[nodiscard]] result<> cmd_set_chip_param(uint8_t channel,
                                            const can_bit_timing& t) {
    std::array<uint8_t, 104> buf{};
    set_le32(buf.data(), 104);
    set_le32(buf.data() + 4, vector::CMD_SET_CHIP_PARAM_FD_XL);
    set_le32(buf.data() + 16, channel);
    set_le32(buf.data() + 20, t.bitrate_bps);
    set_le32(buf.data() + 24, t.sjw);
    set_le32(buf.data() + 28, t.tseg1);
    set_le32(buf.data() + 32, t.tseg2);
    set_le32(buf.data() + 64, 1);

    if (debug()) {
      std::fprintf(stderr,
                   "[vector] SetChipParam: ch=%u bitrate=%u sjw=%u "
                   "tseg1=%u tseg2=%u (TQ=%u, brp=%u)\n",
                   channel, t.bitrate_bps, t.sjw, t.tseg1, t.tseg2,
                   1 + t.tseg1 + t.tseg2,
                   core_clock_hz_ / (t.bitrate_bps * (1 + t.tseg1 + t.tseg2)));
    }

    return send_sync_cmd(buf.data(), 104);
  }

  [[nodiscard]] result<> cmd_set_transceiver_mode(uint8_t channel,
                                                  uint32_t mode) {
    std::array<uint8_t, 24> buf{};
    set_le32(buf.data(), 24);
    set_le32(buf.data() + 4, vector::CMD_SET_TRANSCEIVER_MODE);

    set_le32(buf.data() + 16, channel);

    set_le32(buf.data() + 20, mode);
    return send_sync_cmd(buf.data(), 24);
  }

  [[nodiscard]] result<> cmd_activate_channel(uint8_t channel) {
    std::array<uint8_t, 24> buf{};
    set_le32(buf.data(), 24);
    set_le32(buf.data() + 4, vector::CMD_ACTIVATE_CHANNEL);

    set_le32(buf.data() + 16, channel);
    return send_sync_cmd(buf.data(), 24);
  }

  [[nodiscard]] result<> cmd_deactivate_channel(uint8_t channel) {
    std::array<uint8_t, 24> buf{};
    set_le32(buf.data(), 24);
    set_le32(buf.data() + 4, vector::CMD_DEACTIVATE_CHANNEL);

    set_le32(buf.data() + 16, channel);
    return send_sync_cmd(buf.data(), 24);
  }

  [[nodiscard]] result<> run_init_sequence(slcan_bitrate bitrate) {
    if (debug()) std::fprintf(stderr, "[vector] === init sequence start ===\n");

    if (auto r = cmd_get_bootcode_info(); !r) {
      if (debug()) std::fprintf(stderr, "[vector] get_bootcode_info failed\n");
      return r;
    }
    if (debug()) std::fprintf(stderr, "[vector] bootcode info OK\n");

    if (debug())
      std::fprintf(stderr, "[vector] downloading firmware (%zu bytes)...\n",
                   vector::firmware::main_fw_size());
    if (auto r = download_firmware_blob(vector::CMD_DOWNLOAD_FW_CHUNK,
                                        vector::firmware::main_fw(),
                                        vector::firmware::main_fw_size());
        !r) {
      if (debug()) std::fprintf(stderr, "[vector] firmware download FAILED\n");
      return r;
    }
    if (debug()) std::fprintf(stderr, "[vector] firmware download OK\n");

    for (int attempt = 0; attempt < 10; ++attempt) {
      if (auto r = cmd_get_firmware_info(); r) break;
      if (attempt == 9) {
        if (debug())
          std::fprintf(stderr,
                       "[vector] firmware did not start after 10 retries\n");
        return std::unexpected(error_code::read_error);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (debug()) std::fprintf(stderr, "[vector] firmware is running\n");

    if (debug())
      std::fprintf(stderr, "[vector] downloading FPGA (%zu bytes)...\n",
                   vector::firmware::fpga_size());
    if (auto r = download_firmware_blob(vector::CMD_DOWNLOAD_FPGA_CHUNK,
                                        vector::firmware::fpga(),
                                        vector::firmware::fpga_size());
        !r) {
      if (debug()) std::fprintf(stderr, "[vector] FPGA download FAILED\n");
      return r;
    }
    if (debug()) std::fprintf(stderr, "[vector] FPGA download OK\n");

    if (auto r = cmd_get_firmware_info(); !r) {
      if (debug())
        std::fprintf(stderr, "[vector] get_firmware_info post-FPGA failed\n");
      return r;
    }

    if (auto r = cmd_set_rx_evt_transfer_mode(1, 0); !r) {
      if (debug())
        std::fprintf(stderr, "[vector] set_rx_evt_transfer_mode failed\n");
      return r;
    }

    {
      std::array<uint8_t, 16> ti_buf{};
      set_le32(ti_buf.data(), 16);
      set_le32(ti_buf.data() + 4, vector::CMD_GET_TRANSCEIVER_INFO);
      (void)send_sync_cmd(ti_buf.data(), 16);
      if (debug())
        std::fprintf(stderr, "[vector] waiting for FPGA transceiver init...\n");
    }

    {
      std::array<uint8_t, 4096> evt_buf{};
      int xfer = 0;
      bool got_event = false;
      for (int i = 0; i < 10; ++i) {
        int r =
            libusb_bulk_transfer(dev_, vector::k_ep_rx_data_in, evt_buf.data(),
                                 static_cast<int>(evt_buf.size()), &xfer, 500);
        if (r == 0 && xfer > 0) {
          if (debug())
            std::fprintf(
                stderr,
                "[vector] got event (%d bytes) during FPGA wait (iter %d)\n",
                xfer, i);
          got_event = true;
          break;
        }
        if (r == LIBUSB_ERROR_TIMEOUT) continue;
        break;
      }
      if (debug())
        std::fprintf(stderr, "[vector] FPGA wait done (event=%s)\n",
                     got_event ? "yes" : "no");
    }

    if (auto r = cmd_start_firmware_op(); !r) {
      if (debug()) std::fprintf(stderr, "[vector] start_firmware_op failed\n");
      return r;
    }

    if (auto r = cmd_read_core_frequency(); !r) {
      if (debug())
        std::fprintf(stderr,
                     "[vector] read_core_frequency failed (non-fatal)\n");
    }

    if (auto r = cmd_read_canxl_caps(); !r) {
      if (debug())
        std::fprintf(stderr, "[vector] read_canxl_caps failed (non-fatal)\n");
    }

    for (uint8_t ch = 0; ch < 5; ++ch) {
      if (auto r = cmd_read_default_config(ch); !r) {
        if (debug())
          std::fprintf(stderr,
                       "[vector] read_default_config ch%u failed (non-fatal)\n",
                       ch);
      }
    }

    static constexpr uint32_t bitrate_bps_map[] = {
        10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000,
    };
    uint32_t br_bps =
        bitrate_bps_map[static_cast<unsigned>(bitrate) %
                        (sizeof(bitrate_bps_map) / sizeof(bitrate_bps_map[0]))];

    if (core_clock_hz_ == 0) {
      if (debug())
        std::fprintf(stderr,
                     "[vector] WARNING: core clock unknown, using 160 MHz\n");
      core_clock_hz_ = 160'000'000;
    }
    auto timing = compute_can_timing(core_clock_hz_, br_bps);

    if (auto r = cmd_set_output_mode(channel_, vector::OUTPUT_MODE_NORMAL);
        !r) {
      if (debug()) std::fprintf(stderr, "[vector] set_output_mode failed\n");
      return r;
    }

    if (auto r = cmd_set_chip_param(channel_, timing); !r) {
      if (debug()) std::fprintf(stderr, "[vector] set_chip_param failed\n");
      return r;
    }

    if (auto r =
            cmd_set_transceiver_mode(channel_, vector::TRANSCEIVER_MODE_NORMAL);
        !r) {
      if (debug())
        std::fprintf(stderr, "[vector] set_transceiver_mode failed\n");
      return r;
    }

    if (auto r = cmd_activate_channel(channel_); !r) {
      if (debug()) std::fprintf(stderr, "[vector] activate_channel failed\n");
      return r;
    }

    if (debug())
      std::fprintf(stderr, "[vector] === init sequence complete ===\n");

    return {};
  }

  void parse_rx_event(const uint8_t* data, uint16_t size,
                      std::vector<can_frame>& out) {
    if (size < 24) return;

    uint16_t tag = get_le16(data + 2);
    uint8_t ch = data[13];

    if (tag == vector::FW_CANFD_RX_OK) {
      if (size < 40) return;

      can_frame f{};
      f.timestamp = can_frame::clock::now();

      uint32_t msg_ctrl = get_le32(data + 0x20);
      uint32_t can_id_raw = get_le32(data + 0x24);

      f.dlc = static_cast<uint8_t>(msg_ctrl & 0x0F);

      f.fd = (msg_ctrl & (1u << 29)) != 0;

      f.brs = (msg_ctrl & (1u << 30)) != 0;

      f.id = can_id_raw & 0x1FFFFFFF;

      f.extended = (can_id_raw & (1u << 29)) != 0;
      if (!f.extended) f.id &= 0x7FF;

      f.rtr = (msg_ctrl & (1u << 4)) != 0;

      uint8_t payload_len = frame_payload_len(f);
      size_t data_offset = (size >= 96) ? 0x40 : 0x28;
      if (data_offset + payload_len <= size) {
        std::memcpy(f.data.data(), data + data_offset, payload_len);
      }

      if (debug()) {
        std::fprintf(stderr, "[vector] RX: ch=%u id=0x%X dlc=%u fd=%d size=%u",
                     ch, f.id, f.dlc, f.fd, size);
        for (uint8_t i = 0; i < std::min(payload_len, uint8_t{8}); ++i)
          std::fprintf(stderr, " %02X", f.data[i]);
        std::fprintf(stderr, "\n");
      }

      out.push_back(f);
    } else if (tag == vector::FW_CANFD_TX_OK ||
               tag == vector::FW_CANFD_TX_RECEIPT) {
      if (debug())
        std::fprintf(stderr, "[vector] TX ack: tag=0x%04X ch=%u\n", tag, ch);
    } else if (tag == vector::FW_CANFD_RX_ERROR ||
               tag == vector::FW_CANFD_TX_ERROR) {
      if (debug())
        std::fprintf(stderr, "[vector] error event: tag=0x%04X ch=%u\n", tag,
                     ch);
    } else if (tag == vector::XL_TIMER_EVENT || tag == vector::XL_SYNC_PULSE) {
    } else if (tag >= 0x0400 && tag <= 0x0A0D) {
      if (debug())
        std::fprintf(stderr, "[vector] event: tag=0x%04X ch=%u size=%u\n", tag,
                     ch, size);
    }
  }
};

}  // namespace jcan
