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
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "types.hpp"

namespace jcan {

namespace kvaser {

inline constexpr uint16_t k_vid = 0x0BFD;

struct known_pid {
  uint16_t pid;
  const char* name;
  uint8_t channels;
};

inline constexpr known_pid k_leaf_pids[] = {
    {10, "Leaf prototype", 1},
    {11, "Leaf Light", 1},
    {12, "Leaf Professional HS", 1},
    {14, "Leaf SemiPro HS", 1},
    {15, "Leaf Professional LS", 1},
    {16, "Leaf Professional SWC", 1},
    {17, "Leaf Professional LIN", 1},
    {18, "Leaf SemiPro LS", 1},
    {19, "Leaf SemiPro SWC", 1},
    {22, "Memorator II Prototype", 2},
    {23, "Memorator II HS/HS", 2},
    {24, "USBcan Professional HS/HS", 2},
    {25, "Leaf Light GI", 1},
    {26, "Leaf Professional HS (OBD-II)", 1},
    {27, "Memorator Professional HS/LS", 2},
    {28, "Leaf Light China", 1},
    {29, "BlackBird SemiPro", 1},
    {32, "Memorator R SemiPro", 1},
    {34, "OEM Mercury", 1},
    {35, "OEM Leaf", 1},
    {38, "Key Driving Interface HS", 1},
    {39, "USBcan R", 1},
    {288, "Leaf Light v2", 1},
    {289, "Mini PCI Express HS", 1},
    {290, "Leaf Light HS v2 OEM", 1},
    {291, "USBcan Light 2xHS", 2},
    {292, "Mini PCI Express 2xHS", 2},
    {294, "USBcan R v2", 1},
    {295, "Leaf Light R v2", 1},
    {296, "OEM ATI Leaf Light HS v2", 1},
};

inline constexpr known_pid k_mhydra_pids[] = {
    {256, "Eagle", 2},
    {258, "BlackBird v2", 1},
    {260, "Memorator Pro 5xHS", 5},
    {261, "USBcan Pro 5xHS", 5},
    {262, "USBcan Light 4xHS", 4},
    {263, "Leaf Pro HS v2", 1},
    {264, "USBcan Pro 2xHS v2", 2},
    {265, "Memorator 2xHS v2", 2},
    {266, "Memorator Pro 2xHS v2", 2},
    {267, "Hybrid 2xCAN/LIN", 2},
    {268, "ATI USBcan Pro 2xHS v2", 2},
    {269, "ATI Memorator Pro 2xHS v2", 2},
    {270, "Hybrid Pro 2xCAN/LIN", 2},
    {271, "BlackBird Pro HS v2", 1},
    {272, "Memorator Light HS v2", 1},
    {273, "U100", 1},
    {274, "U100P", 1},
    {275, "U100S", 1},
    {276, "USBcan Pro 4xHS", 4},
    {277, "Hybrid CAN/LIN", 1},
    {278, "Hybrid Pro CAN/LIN", 1},
    {279, "Leaf v3", 1},
    {280, "USBcan Pro 4xCAN Silent", 4},
    {281, "VINING 800", 2},
    {282, "USBcan Pro 5xCAN", 5},
    {283, "Mini PCIe 1xCAN", 1},
    {284, "Easyscan CAN", 1},
    {285, "CAN Logger Read Only", 1},
};

inline const known_pid* find_leaf(uint16_t pid) {
  for (const auto& e : k_leaf_pids)
    if (e.pid == pid) return &e;
  return nullptr;
}

inline const known_pid* find_mhydra(uint16_t pid) {
  for (const auto& e : k_mhydra_pids)
    if (e.pid == pid) return &e;
  return nullptr;
}

inline const known_pid* find_any(uint16_t pid) {
  auto* p = find_leaf(pid);
  if (p) return p;
  return find_mhydra(pid);
}

inline bool is_mhydra_pid(uint16_t pid) { return find_mhydra(pid) != nullptr; }

inline constexpr unsigned k_cmd_timeout_ms = 2000;
inline constexpr unsigned k_rx_timeout_ms = 50;
inline constexpr unsigned k_tx_timeout_ms = 500;

inline constexpr uint8_t CMD_RX_STD_MESSAGE = 12;
inline constexpr uint8_t CMD_TX_STD_MESSAGE = 13;
inline constexpr uint8_t CMD_RX_EXT_MESSAGE = 14;
inline constexpr uint8_t CMD_TX_EXT_MESSAGE = 15;
inline constexpr uint8_t CMD_SET_BUSPARAMS_REQ = 16;
inline constexpr uint8_t CMD_GET_BUSPARAMS_REQ = 17;
inline constexpr uint8_t CMD_GET_BUSPARAMS_RESP = 18;
inline constexpr uint8_t CMD_GET_CHIP_STATE_REQ = 19;
inline constexpr uint8_t CMD_CHIP_STATE_EVENT = 20;
inline constexpr uint8_t CMD_SET_DRIVERMODE_REQ = 21;
inline constexpr uint8_t CMD_START_CHIP_REQ = 26;
inline constexpr uint8_t CMD_START_CHIP_RESP = 27;
inline constexpr uint8_t CMD_STOP_CHIP_REQ = 28;
inline constexpr uint8_t CMD_STOP_CHIP_RESP = 29;
inline constexpr uint8_t CMD_TX_CAN_MESSAGE = 33;
inline constexpr uint8_t CMD_GET_CARD_INFO_REQ = 34;
inline constexpr uint8_t CMD_GET_CARD_INFO_RESP = 35;
inline constexpr uint8_t CMD_GET_SOFTWARE_INFO_REQ = 38;
inline constexpr uint8_t CMD_GET_SOFTWARE_INFO_RESP = 39;
inline constexpr uint8_t CMD_TX_ACKNOWLEDGE = 50;
inline constexpr uint8_t CMD_RESET_CHIP_REQ = 24;
inline constexpr uint8_t CMD_ERROR_EVENT = 45;
inline constexpr uint8_t CMD_SET_BUSPARAMS_RESP = 85;
inline constexpr uint8_t CMD_SET_TRANSCEIVER_MODE_REQ = 95;
inline constexpr uint8_t CMD_LOG_MESSAGE = 106;

inline constexpr uint8_t CMD_MAP_CHANNEL_REQ = 200;
inline constexpr uint8_t CMD_MAP_CHANNEL_RESP = 201;
inline constexpr uint8_t CMD_GET_SOFTWARE_DETAILS_REQ = 202;
inline constexpr uint8_t CMD_GET_SOFTWARE_DETAILS_RESP = 203;
inline constexpr uint8_t CMD_EXTENDED = 255;

inline constexpr uint8_t CMD_TX_CAN_MESSAGE_FD = 224;
inline constexpr uint8_t CMD_TX_ACKNOWLEDGE_FD = 225;
inline constexpr uint8_t CMD_RX_MESSAGE_FD = 226;

inline constexpr uint8_t MSGFLAG_ERROR_FRAME = 0x01;
inline constexpr uint8_t MSGFLAG_OVERRUN = 0x02;
inline constexpr uint8_t MSGFLAG_REMOTE_FRAME = 0x10;
inline constexpr uint8_t MSGFLAG_EXTENDED_ID = 0x20;
inline constexpr uint8_t MSGFLAG_TX = 0x40;

inline constexpr uint32_t MSGFLAG_FDF = 0x010000;
inline constexpr uint32_t MSGFLAG_BRS = 0x020000;
inline constexpr uint32_t MSGFLAG_ESI = 0x040000;

inline constexpr uint8_t DRIVERMODE_NORMAL = 1;
inline constexpr uint8_t DRIVERMODE_SILENT = 2;

inline constexpr uint32_t MAX_CMD_LEN = 32;
inline constexpr uint32_t MAX_PACKET_SIZE = 3072;
inline constexpr uint32_t HYDRA_CMD_SIZE = 32;
inline constexpr uint32_t MAX_PACKET_OUT = 3072;
inline constexpr uint32_t MAX_PACKET_IN = 4096;

inline constexpr uint8_t ROUTER_HE = 0x00;
inline constexpr uint8_t ILLEGAL_HE = 0x3E;
inline constexpr uint8_t BROADCAST_HE = 0x0F;
inline constexpr uint8_t MAX_HE_COUNT = 64;
inline constexpr uint8_t HYDRA_MAX_CARD_CHANNELS = 5;

inline constexpr unsigned HE_BITS = 4;
inline constexpr unsigned CH_BITS = 2;
inline constexpr unsigned SEQ_BITS = 12;
inline constexpr uint16_t SEQ_MASK = (1u << SEQ_BITS) - 1;
inline constexpr uint8_t ADDR_MASK = (1u << (HE_BITS + CH_BITS)) - 1;
inline constexpr uint8_t CH_HI_MASK =
    static_cast<uint8_t>(((1u << CH_BITS) - 1) << (HE_BITS + CH_BITS));

inline void hydra_set_dst(uint8_t* cmd, uint8_t dst) {
  cmd[1] = (cmd[1] & CH_HI_MASK) | (dst & ADDR_MASK);
}
inline void hydra_set_src(uint8_t* cmd, uint8_t src) {
  cmd[1] = (cmd[1] & ADDR_MASK) |
           static_cast<uint8_t>((src << CH_BITS) & CH_HI_MASK);
  uint16_t tid =
      static_cast<uint16_t>(cmd[2]) | (static_cast<uint16_t>(cmd[3]) << 8);
  tid = (tid & SEQ_MASK) | static_cast<uint16_t>(src << SEQ_BITS);
  cmd[2] = static_cast<uint8_t>(tid);
  cmd[3] = static_cast<uint8_t>(tid >> 8);
}
inline void hydra_set_seq(uint8_t* cmd, uint16_t seq) {
  uint16_t tid =
      static_cast<uint16_t>(cmd[2]) | (static_cast<uint16_t>(cmd[3]) << 8);
  tid = (tid & ~SEQ_MASK) | (seq & SEQ_MASK);
  cmd[2] = static_cast<uint8_t>(tid);
  cmd[3] = static_cast<uint8_t>(tid >> 8);
}
inline uint8_t hydra_get_src(const uint8_t* cmd) {
  uint16_t tid =
      static_cast<uint16_t>(cmd[2]) | (static_cast<uint16_t>(cmd[3]) << 8);
  return static_cast<uint8_t>(
      (((cmd[1] & CH_HI_MASK) >> CH_BITS) | (tid >> SEQ_BITS)) & ADDR_MASK);
}
inline uint16_t hydra_get_seq(const uint8_t* cmd) {
  uint16_t tid =
      static_cast<uint16_t>(cmd[2]) | (static_cast<uint16_t>(cmd[3]) << 8);
  return tid & SEQ_MASK;
}

inline constexpr uint32_t fpga_id(uint32_t can_id, bool ext, bool rtr) {
  uint32_t w = can_id & 0x1FFFFFFF;
  if (ext) w |= (1u << 30) | (1u << 31);
  if (rtr) w |= (1u << 29);
  return w;
}
inline constexpr uint32_t fpga_control(uint8_t dlc, bool areq) {
  uint32_t w = (static_cast<uint32_t>(dlc & 0xF) << 8);
  if (areq) w |= (1u << 31);
  return w;
}

inline constexpr uint8_t EP_IN_KDI = 0x81;
inline constexpr uint8_t EP_IN_CMD = 0x82;
inline constexpr uint8_t EP_IN_FAT = 0x83;

inline constexpr uint32_t SWOPTION_USE_HYDRA_EXT = 0x200;
inline constexpr uint32_t SWOPTION_CANFD_CAP = 0x400;
inline constexpr uint32_t SWOPTION_80_MHZ_CLK = 0x20;
inline constexpr uint32_t SWOPTION_24_MHZ_CLK = 0x40;
inline constexpr uint32_t SWOPTION_CPU_FQ_MASK = 0x60;
inline constexpr uint32_t SWOPTION_80_MHZ_CAN_CLK = 0x2000;
inline constexpr uint32_t SWOPTION_24_MHZ_CAN_CLK = 0x4000;
inline constexpr uint32_t SWOPTION_CAN_CLK_MASK = 0x6000;

}  // namespace kvaser

struct kvaser_usb {
  libusb_context* ctx_{nullptr};
  libusb_device_handle* dev_{nullptr};
  bool open_{false};
  uint8_t channel_{0};
  uint8_t ep_bulk_in_{0};
  uint8_t ep_bulk_out_{0};
  uint16_t max_packet_in_{64};
  uint16_t max_packet_out_{64};
  uint8_t channel_count_{1};
  uint16_t max_outstanding_tx_{0};
  uint8_t trans_id_{1};

  bool is_mhydra_{false};
  bool use_hydra_ext_{false};
  uint8_t channel2he_[kvaser::HYDRA_MAX_CARD_CHANNELS]{};
  uint8_t he2channel_[kvaser::MAX_HE_COUNT]{};
  uint32_t can_clock_mhz_{80};
  uint8_t ep_cmd_in_{0};

  static bool debug() { return std::getenv("JCAN_DEBUG") != nullptr; }

  [[nodiscard]] result<> open(const std::string& port,
                              slcan_bitrate bitrate = slcan_bitrate::s6,
                              [[maybe_unused]] unsigned baud = 0) {
    if (open_) return std::unexpected(error_code::already_open);

    uint16_t target_pid = 0;
    channel_ = 0;

    {
      auto colon = port.find(':');
      if (colon != std::string::npos) {
        channel_ =
            static_cast<uint8_t>(std::atoi(port.substr(colon + 1).c_str()));
      }

      std::string pid_part =
          (colon != std::string::npos) ? port.substr(0, colon) : port;
      if (!pid_part.empty() &&
          std::isdigit(static_cast<unsigned char>(pid_part[0]))) {
        target_pid = static_cast<uint16_t>(std::atoi(pid_part.c_str()));
      }
    }

    int r = libusb_init(&ctx_);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[kvaser] libusb_init failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
      return std::unexpected(error_code::port_open_failed);
    }

    if (target_pid != 0) {
      dev_ = libusb_open_device_with_vid_pid(ctx_, kvaser::k_vid, target_pid);
    } else {
      for (const auto& kp : kvaser::k_mhydra_pids) {
        dev_ = libusb_open_device_with_vid_pid(ctx_, kvaser::k_vid, kp.pid);
        if (dev_) {
          target_pid = kp.pid;
          break;
        }
      }
      if (!dev_) {
        for (const auto& kp : kvaser::k_leaf_pids) {
          dev_ = libusb_open_device_with_vid_pid(ctx_, kvaser::k_vid, kp.pid);
          if (dev_) {
            target_pid = kp.pid;
            break;
          }
        }
      }
    }

    if (!dev_) {
      bool seen = false;
      libusb_device** list = nullptr;
      ssize_t cnt = libusb_get_device_list(ctx_, &list);
      for (ssize_t i = 0; i < cnt; ++i) {
        struct libusb_device_descriptor desc{};
        libusb_get_device_descriptor(list[i], &desc);
        if (desc.idVendor == kvaser::k_vid &&
            kvaser::find_any(desc.idProduct)) {
          seen = true;
          break;
        }
      }
      if (list) libusb_free_device_list(list, 1);

      if (seen) {
        if (debug())
          std::fprintf(stderr,
                       "[kvaser] device found but cannot open — permission "
                       "denied. add a udev rule:\n"
                       "  echo 'SUBSYSTEM==\"usb\", ATTR{idVendor}==\"0bfd\", "
                       "MODE=\"0666\"' | sudo tee "
                       "/etc/udev/rules.d/99-kvaser.rules\n"
                       "  sudo udevadm control --reload-rules && sudo udevadm "
                       "trigger\n");
        libusb_exit(ctx_);
        ctx_ = nullptr;
        return std::unexpected(error_code::permission_denied);
      }

      if (debug()) std::fprintf(stderr, "[kvaser] no device found\n");
      libusb_exit(ctx_);
      ctx_ = nullptr;
      return std::unexpected(error_code::port_not_found);
    }

    is_mhydra_ = kvaser::is_mhydra_pid(target_pid);

    if (libusb_kernel_driver_active(dev_, 0) == 1)
      libusb_detach_kernel_driver(dev_, 0);

    r = libusb_claim_interface(dev_, 0);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[kvaser] claim interface failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
      libusb_close(dev_);
      libusb_exit(ctx_);
      dev_ = nullptr;
      ctx_ = nullptr;
      return std::unexpected(error_code::permission_denied);
    }

    if (is_mhydra_) {
      if (auto res = discover_endpoints_mhydra(); !res) {
        libusb_release_interface(dev_, 0);
        libusb_close(dev_);
        libusb_exit(ctx_);
        dev_ = nullptr;
        ctx_ = nullptr;
        return res;
      }
    } else {
      if (auto res = discover_endpoints(); !res) {
        libusb_release_interface(dev_, 0);
        libusb_close(dev_);
        libusb_exit(ctx_);
        dev_ = nullptr;
        ctx_ = nullptr;
        return res;
      }
    }

    flush_rx();

    if (is_mhydra_) {
      if (auto res = run_init_sequence_mhydra(bitrate); !res) {
        libusb_release_interface(dev_, 0);
        libusb_close(dev_);
        libusb_exit(ctx_);
        dev_ = nullptr;
        ctx_ = nullptr;
        return res;
      }
    } else {
      if (auto res = run_init_sequence(bitrate); !res) {
        libusb_release_interface(dev_, 0);
        libusb_close(dev_);
        libusb_exit(ctx_);
        dev_ = nullptr;
        ctx_ = nullptr;
        return res;
      }
    }

    open_ = true;
    if (debug()) {
      auto* kp = kvaser::find_any(target_pid);
      std::fprintf(stderr, "[kvaser] opened %s (PID %u) channel %u [%s]\n",
                   kp ? kp->name : "unknown", target_pid, channel_,
                   is_mhydra_ ? "mhydra" : "leaf");
    }
    return {};
  }

  [[nodiscard]] result<> close() {
    if (!open_) return std::unexpected(error_code::not_open);

    if (is_mhydra_) {
      (void)mhydra_cmd_stop_chip();
    } else {
      (void)cmd_stop_chip(channel_);
    }

    libusb_release_interface(dev_, 0);
    libusb_close(dev_);
    libusb_exit(ctx_);
    dev_ = nullptr;
    ctx_ = nullptr;
    open_ = false;
    if (debug()) std::fprintf(stderr, "[kvaser] closed\n");
    return {};
  }

  [[nodiscard]] result<> send(const can_frame& frame) {
    if (!open_) return std::unexpected(error_code::not_open);
    return is_mhydra_ ? send_mhydra(frame) : send_leaf(frame);
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
    return is_mhydra_ ? recv_many_mhydra(timeout_ms)
                      : recv_many_leaf(timeout_ms);
  }

 private:
  uint8_t next_trans_id() {
    uint8_t id = trans_id_++;
    if (trans_id_ == 0) trans_id_ = 1;
    return id;
  }

  void flush_rx() {
    std::array<uint8_t, 4096> buf{};
    int xfer = 0;
    uint8_t ep = is_mhydra_ ? ep_cmd_in_ : ep_bulk_in_;
    for (int i = 0; i < 8; ++i) {
      int r = libusb_bulk_transfer(dev_, ep, buf.data(),
                                   static_cast<int>(buf.size()), &xfer, 50);
      if (r == LIBUSB_ERROR_TIMEOUT || xfer == 0) break;
      if (debug())
        std::fprintf(stderr, "[kvaser] flushed %d stale bytes\n", xfer);
    }
  }

  [[nodiscard]] result<> send_leaf(const can_frame& frame) {
    std::array<uint8_t, 32> cmd{};

    bool ext = frame.extended;
    cmd[0] = 20;
    cmd[1] = ext ? kvaser::CMD_TX_EXT_MESSAGE : kvaser::CMD_TX_STD_MESSAGE;
    cmd[2] = channel_;
    cmd[3] = next_trans_id();

    uint8_t* raw = &cmd[4];

    if (ext) {
      raw[0] = static_cast<uint8_t>((frame.id >> 24) & 0x1F);
      raw[1] = static_cast<uint8_t>((frame.id >> 18) & 0x3F);
      raw[2] = static_cast<uint8_t>((frame.id >> 14) & 0x0F);
      raw[3] = static_cast<uint8_t>((frame.id >> 6) & 0xFF);
      raw[4] = static_cast<uint8_t>(frame.id & 0x3F);
    } else {
      raw[0] = static_cast<uint8_t>((frame.id >> 6) & 0x1F);
      raw[1] = static_cast<uint8_t>(frame.id & 0x3F);
    }

    raw[5] = frame.dlc & 0x0F;
    uint8_t len = std::min(frame.dlc, static_cast<uint8_t>(8));
    std::memcpy(&raw[6], frame.data.data(), len);

    cmd[19] = frame.rtr ? kvaser::MSGFLAG_REMOTE_FRAME : 0;

    return leaf_send_cmd(cmd.data(), 20);
  }

  [[nodiscard]] result<std::vector<can_frame>> recv_many_leaf(
      unsigned timeout_ms) {
    std::vector<can_frame> frames;
    std::array<uint8_t, 3072> buf{};

    int transferred = 0;
    int r = libusb_bulk_transfer(dev_, ep_bulk_in_, buf.data(),
                                 static_cast<int>(buf.size()), &transferred,
                                 static_cast<unsigned>(timeout_ms));

    if (r == LIBUSB_ERROR_TIMEOUT) return frames;
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[kvaser] RX failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
      return std::unexpected(error_code::read_error);
    }

    size_t pos = 0;
    size_t total = static_cast<size_t>(transferred);

    while (pos < total) {
      uint8_t cmd_len = buf[pos];

      if (cmd_len == 0) {
        pos =
            (pos + max_packet_in_) & ~(static_cast<size_t>(max_packet_in_) - 1);
        continue;
      }

      if (cmd_len < 2 || pos + cmd_len > total) break;

      uint8_t cmd_no = buf[pos + 1];

      if (cmd_no == kvaser::CMD_RX_STD_MESSAGE ||
          cmd_no == kvaser::CMD_RX_EXT_MESSAGE) {
        if (cmd_len >= 24) {
          leaf_parse_rx_frame(&buf[pos], cmd_no, frames);
        }
      } else if (cmd_no == kvaser::CMD_CHIP_STATE_EVENT) {
        if (debug()) std::fprintf(stderr, "[kvaser] chip state event\n");
      } else if (cmd_no == kvaser::CMD_ERROR_EVENT) {
        if (debug()) std::fprintf(stderr, "[kvaser] error event\n");
      } else if (cmd_no == kvaser::CMD_LOG_MESSAGE) {
      } else if (cmd_no == kvaser::CMD_START_CHIP_RESP ||
                 cmd_no == kvaser::CMD_STOP_CHIP_RESP) {
      } else {
        if (debug())
          std::fprintf(stderr, "[kvaser] cmd %u len %u\n", cmd_no, cmd_len);
      }

      pos += cmd_len;
    }

    return frames;
  }

  [[nodiscard]] result<> discover_endpoints() {
    libusb_device* udev = libusb_get_device(dev_);
    struct libusb_config_descriptor* config = nullptr;
    int r = libusb_get_active_config_descriptor(udev, &config);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[kvaser] get config descriptor failed\n");
      return std::unexpected(error_code::port_open_failed);
    }

    bool found_in = false, found_out = false;

    for (int i = 0; i < config->bNumInterfaces && (!found_in || !found_out);
         ++i) {
      const auto& iface = config->interface[i];
      for (int j = 0; j < iface.num_altsetting; ++j) {
        const auto& alt = iface.altsetting[j];
        for (int k = 0; k < alt.bNumEndpoints; ++k) {
          const auto& ep = alt.endpoint[k];
          if ((ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) !=
              LIBUSB_TRANSFER_TYPE_BULK)
            continue;
          if (!found_in && (ep.bEndpointAddress & LIBUSB_ENDPOINT_IN)) {
            ep_bulk_in_ = ep.bEndpointAddress;
            max_packet_in_ = ep.wMaxPacketSize;
            found_in = true;
          } else if (!found_out &&
                     !(ep.bEndpointAddress & LIBUSB_ENDPOINT_IN)) {
            ep_bulk_out_ = ep.bEndpointAddress;
            max_packet_out_ = ep.wMaxPacketSize;
            found_out = true;
          }
        }
      }
    }

    libusb_free_config_descriptor(config);

    if (!found_in || !found_out) {
      if (debug())
        std::fprintf(stderr, "[kvaser] could not find bulk endpoints\n");
      return std::unexpected(error_code::port_open_failed);
    }

    if (debug())
      std::fprintf(stderr,
                   "[kvaser] endpoints: IN=0x%02X (%u) OUT=0x%02X (%u)\n",
                   ep_bulk_in_, max_packet_in_, ep_bulk_out_, max_packet_out_);
    return {};
  }

  [[nodiscard]] result<> leaf_send_cmd(const uint8_t* cmd, uint8_t len) {
    std::array<uint8_t, 64> buf{};
    std::memcpy(buf.data(), cmd, len);

    int transferred = 0;
    int r = libusb_bulk_transfer(dev_, ep_bulk_out_, buf.data(),
                                 static_cast<int>(len), &transferred,
                                 kvaser::k_cmd_timeout_ms);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[kvaser] send_cmd(%u) failed: %s\n", cmd[1],
                     libusb_strerror(static_cast<libusb_error>(r)));
      return std::unexpected(error_code::write_error);
    }
    return {};
  }

  [[nodiscard]] result<> leaf_send_cmd_wait(const uint8_t* cmd, uint8_t len,
                                            uint8_t resp_cmd_no,
                                            uint8_t* resp_buf,
                                            uint8_t resp_max) {
    if (auto r = leaf_send_cmd(cmd, len); !r) return r;

    std::array<uint8_t, 3072> buf{};
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(kvaser::k_cmd_timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
      int transferred = 0;
      int r =
          libusb_bulk_transfer(dev_, ep_bulk_in_, buf.data(),
                               static_cast<int>(buf.size()), &transferred, 500);
      if (r == LIBUSB_ERROR_TIMEOUT) continue;
      if (r < 0) return std::unexpected(error_code::read_error);

      size_t pos = 0;
      size_t total = static_cast<size_t>(transferred);
      while (pos < total) {
        uint8_t cl = buf[pos];
        if (cl == 0) {
          pos = (pos + max_packet_in_) &
                ~(static_cast<size_t>(max_packet_in_) - 1);
          continue;
        }
        if (cl < 2 || pos + cl > total) break;
        if (buf[pos + 1] == resp_cmd_no) {
          uint8_t copy_len = std::min(cl, resp_max);
          std::memcpy(resp_buf, &buf[pos], copy_len);
          return {};
        }
        pos += cl;
      }
    }

    if (debug())
      std::fprintf(stderr, "[kvaser] timeout waiting for cmd %u\n",
                   resp_cmd_no);
    return std::unexpected(error_code::read_timeout);
  }

  [[nodiscard]] result<> run_init_sequence(slcan_bitrate bitrate) {
    if (debug()) std::fprintf(stderr, "[kvaser] === leaf init start ===\n");

    {
      std::array<uint8_t, 32> cmd{};
      cmd[0] = 4;
      cmd[1] = kvaser::CMD_GET_SOFTWARE_INFO_REQ;
      cmd[2] = next_trans_id();

      std::array<uint8_t, 32> resp{};
      if (auto r = leaf_send_cmd_wait(cmd.data(), 4,
                                      kvaser::CMD_GET_SOFTWARE_INFO_RESP,
                                      resp.data(), 32);
          !r) {
        if (debug())
          std::fprintf(stderr, "[kvaser] get_software_info failed\n");
        return r;
      }

      max_outstanding_tx_ = static_cast<uint16_t>(resp[12]) |
                            (static_cast<uint16_t>(resp[13]) << 8);
      if (debug()) {
        uint32_t fw = static_cast<uint32_t>(resp[8]) |
                      (static_cast<uint32_t>(resp[9]) << 8) |
                      (static_cast<uint32_t>(resp[10]) << 16) |
                      (static_cast<uint32_t>(resp[11]) << 24);
        std::fprintf(stderr,
                     "[kvaser] firmware version=0x%08X max_outstanding_tx=%u\n",
                     fw, max_outstanding_tx_);
      }
    }

    {
      std::array<uint8_t, 32> cmd{};
      cmd[0] = 4;
      cmd[1] = kvaser::CMD_GET_CARD_INFO_REQ;
      cmd[2] = next_trans_id();

      std::array<uint8_t, 32> resp{};
      if (auto r = leaf_send_cmd_wait(
              cmd.data(), 4, kvaser::CMD_GET_CARD_INFO_RESP, resp.data(), 32);
          !r) {
        if (debug()) std::fprintf(stderr, "[kvaser] get_card_info failed\n");
        return r;
      }

      channel_count_ = resp[3];
      uint32_t serial = static_cast<uint32_t>(resp[4]) |
                        (static_cast<uint32_t>(resp[5]) << 8) |
                        (static_cast<uint32_t>(resp[6]) << 16) |
                        (static_cast<uint32_t>(resp[7]) << 24);
      if (debug())
        std::fprintf(stderr, "[kvaser] channels=%u serial=%u\n", channel_count_,
                     serial);

      if (channel_ >= channel_count_) {
        if (debug())
          std::fprintf(stderr, "[kvaser] channel %u out of range (max %u)\n",
                       channel_, channel_count_ - 1);
        return std::unexpected(error_code::port_config_failed);
      }
    }

    {
      std::array<uint8_t, 32> cmd{};
      cmd[0] = 4;
      cmd[1] = kvaser::CMD_SET_DRIVERMODE_REQ;
      cmd[2] = next_trans_id();
      cmd[3] = channel_;
      if (auto r = leaf_send_cmd(cmd.data(), 8); !r) return r;

      cmd[4] = kvaser::DRIVERMODE_NORMAL;
      if (auto r = leaf_send_cmd(cmd.data(), 8); !r) return r;
      if (debug()) std::fprintf(stderr, "[kvaser] driver mode set to normal\n");
    }

    {
      static constexpr uint32_t bitrate_bps_map[] = {
          10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000,
      };
      uint32_t br = bitrate_bps_map[static_cast<unsigned>(bitrate) %
                                    (sizeof(bitrate_bps_map) /
                                     sizeof(bitrate_bps_map[0]))];

      uint8_t tseg1, tseg2, sjw;
      compute_leaf_timing(br, tseg1, tseg2, sjw);

      std::array<uint8_t, 32> cmd{};
      cmd[0] = 12;
      cmd[1] = kvaser::CMD_SET_BUSPARAMS_REQ;
      cmd[2] = next_trans_id();
      cmd[3] = channel_;
      cmd[4] = static_cast<uint8_t>(br);
      cmd[5] = static_cast<uint8_t>(br >> 8);
      cmd[6] = static_cast<uint8_t>(br >> 16);
      cmd[7] = static_cast<uint8_t>(br >> 24);
      cmd[8] = tseg1;
      cmd[9] = tseg2;
      cmd[10] = sjw;
      cmd[11] = 1;

      if (auto r = leaf_send_cmd(cmd.data(), 12); !r) {
        if (debug()) std::fprintf(stderr, "[kvaser] set_busparams failed\n");
        return r;
      }
      if (debug())
        std::fprintf(
            stderr, "[kvaser] busparams: bitrate=%u tseg1=%u tseg2=%u sjw=%u\n",
            br, tseg1, tseg2, sjw);
    }

    if (auto r = cmd_start_chip(channel_); !r) {
      if (debug()) std::fprintf(stderr, "[kvaser] start_chip failed\n");
      return r;
    }

    if (debug()) std::fprintf(stderr, "[kvaser] === leaf init complete ===\n");
    return {};
  }

  static void compute_leaf_timing(uint32_t bitrate, uint8_t& tseg1,
                                  uint8_t& tseg2, uint8_t& sjw) {
    constexpr uint32_t clock_hz = 16'000'000;

    for (uint32_t tq = 25; tq >= 3; --tq) {
      uint32_t product = bitrate * tq;
      if (product == 0 || clock_hz % product != 0) continue;
      uint32_t brp = clock_hz / product;
      if (brp < 1 || brp > 64) continue;

      uint32_t t1 = (tq * 80 / 100) - 1;
      uint32_t t2 = tq - 1 - t1;
      if (t1 < 1 || t1 > 16 || t2 < 1 || t2 > 8) continue;

      tseg1 = static_cast<uint8_t>(t1);
      tseg2 = static_cast<uint8_t>(t2);
      sjw = static_cast<uint8_t>(std::min(t2, 4u));
      return;
    }

    tseg1 = 5;
    tseg2 = 2;
    sjw = 1;
  }

  [[nodiscard]] result<> cmd_start_chip(uint8_t channel) {
    std::array<uint8_t, 32> cmd{};
    cmd[0] = 4;
    cmd[1] = kvaser::CMD_START_CHIP_REQ;
    cmd[2] = next_trans_id();
    cmd[3] = channel;

    std::array<uint8_t, 32> resp{};
    return leaf_send_cmd_wait(cmd.data(), 4, kvaser::CMD_START_CHIP_RESP,
                              resp.data(), 32);
  }

  [[nodiscard]] result<> cmd_stop_chip(uint8_t channel) {
    std::array<uint8_t, 32> cmd{};
    cmd[0] = 4;
    cmd[1] = kvaser::CMD_STOP_CHIP_REQ;
    cmd[2] = next_trans_id();
    cmd[3] = channel;

    std::array<uint8_t, 32> resp{};
    return leaf_send_cmd_wait(cmd.data(), 4, kvaser::CMD_STOP_CHIP_RESP,
                              resp.data(), 32);
  }

  void leaf_parse_rx_frame(const uint8_t* data, uint8_t cmd_no,
                           std::vector<can_frame>& out) {
    can_frame f{};
    f.timestamp = can_frame::clock::now();

    uint8_t ch = data[2];
    uint8_t flags = data[3];

    if (flags & kvaser::MSGFLAG_ERROR_FRAME) return;
    if (ch != channel_) return;

    const uint8_t* raw = &data[10];

    if (cmd_no == kvaser::CMD_RX_EXT_MESSAGE) {
      uint32_t id = raw[0] & 0x1F;
      id = (id << 6) | (raw[1] & 0x3F);
      id = (id << 4) | (raw[2] & 0x0F);
      id = (id << 8) | raw[3];
      id = (id << 6) | (raw[4] & 0x3F);
      f.id = id;
      f.extended = true;
    } else {
      uint32_t id = raw[0] & 0x1F;
      id = (id << 6) | (raw[1] & 0x3F);
      f.id = id;
      f.extended = false;
    }

    f.dlc = raw[5] & 0x0F;
    f.rtr = (flags & kvaser::MSGFLAG_REMOTE_FRAME) != 0;

    uint8_t payload = std::min(f.dlc, static_cast<uint8_t>(8));
    std::memcpy(f.data.data(), &raw[6], payload);

    if (debug()) {
      std::fprintf(stderr, "[kvaser] RX: ch=%u id=0x%X%s dlc=%u", ch, f.id,
                   f.extended ? "x" : "", f.dlc);
      for (uint8_t i = 0; i < payload; ++i)
        std::fprintf(stderr, " %02X", f.data[i]);
      std::fprintf(stderr, "\n");
    }

    out.push_back(f);
  }

  [[nodiscard]] result<> discover_endpoints_mhydra() {
    libusb_device* udev = libusb_get_device(dev_);
    struct libusb_config_descriptor* config = nullptr;
    int r = libusb_get_active_config_descriptor(udev, &config);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[kvaser] get config descriptor failed\n");
      return std::unexpected(error_code::port_open_failed);
    }

    ep_cmd_in_ = 0;
    ep_bulk_out_ = 0;

    for (int i = 0; i < config->bNumInterfaces; ++i) {
      const auto& iface = config->interface[i];
      for (int j = 0; j < iface.num_altsetting; ++j) {
        const auto& alt = iface.altsetting[j];
        for (int k = 0; k < alt.bNumEndpoints; ++k) {
          const auto& ep = alt.endpoint[k];
          if ((ep.bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) !=
              LIBUSB_TRANSFER_TYPE_BULK)
            continue;

          if (ep.bEndpointAddress & LIBUSB_ENDPOINT_IN) {
            if (ep.bEndpointAddress == kvaser::EP_IN_CMD) {
              ep_cmd_in_ = ep.bEndpointAddress;
              max_packet_in_ = ep.wMaxPacketSize;
            }
            if (!ep_bulk_in_) {
              ep_bulk_in_ = ep.bEndpointAddress;
            }
          } else {
            if (!ep_bulk_out_) {
              ep_bulk_out_ = ep.bEndpointAddress;
              max_packet_out_ = ep.wMaxPacketSize;
            }
          }
        }
      }
    }

    libusb_free_config_descriptor(config);

    if (!ep_cmd_in_ && ep_bulk_in_) {
      ep_cmd_in_ = ep_bulk_in_;
    }

    if (!ep_cmd_in_ || !ep_bulk_out_) {
      if (debug())
        std::fprintf(stderr, "[kvaser] could not find mhydra bulk endpoints\n");
      return std::unexpected(error_code::port_open_failed);
    }

    if (debug())
      std::fprintf(
          stderr,
          "[kvaser] mhydra endpoints: CMD_IN=0x%02X (%u) OUT=0x%02X (%u)\n",
          ep_cmd_in_, max_packet_in_, ep_bulk_out_, max_packet_out_);
    return {};
  }

  [[nodiscard]] result<> mhydra_send_cmd(uint8_t* cmd) {
    int transferred = 0;
    int r =
        libusb_bulk_transfer(dev_, ep_bulk_out_, cmd, kvaser::HYDRA_CMD_SIZE,
                             &transferred, kvaser::k_cmd_timeout_ms);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[kvaser] mhydra_send_cmd(%u) failed: %s\n",
                     cmd[0], libusb_strerror(static_cast<libusb_error>(r)));
      return std::unexpected(error_code::write_error);
    }
    return {};
  }

  [[nodiscard]] result<> mhydra_send_and_wait(uint8_t* cmd, uint8_t resp_cmd_no,
                                              uint8_t* resp_buf,
                                              size_t resp_max) {
    if (auto r = mhydra_send_cmd(cmd); !r) return r;

    std::array<uint8_t, 4096> buf{};
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(kvaser::k_cmd_timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
      int transferred = 0;
      int r =
          libusb_bulk_transfer(dev_, ep_cmd_in_, buf.data(),
                               static_cast<int>(buf.size()), &transferred, 500);
      if (r == LIBUSB_ERROR_TIMEOUT) continue;
      if (r < 0) return std::unexpected(error_code::read_error);

      size_t pos = 0;
      size_t total = static_cast<size_t>(transferred);

      while (pos + 4 <= total) {
        uint8_t rcmd = buf[pos];
        size_t cmd_sz;

        if (rcmd == kvaser::CMD_EXTENDED && pos + 6 <= total) {
          uint16_t ext_len = static_cast<uint16_t>(buf[pos + 4]) |
                             (static_cast<uint16_t>(buf[pos + 5]) << 8);
          cmd_sz = ext_len;
          if (cmd_sz < 8) cmd_sz = 8;
        } else {
          cmd_sz = kvaser::HYDRA_CMD_SIZE;
        }

        if (pos + cmd_sz > total) break;

        if (rcmd == resp_cmd_no) {
          size_t copy = std::min(cmd_sz, resp_max);
          std::memcpy(resp_buf, &buf[pos], copy);
          return {};
        }
        pos += cmd_sz;
      }
    }

    if (debug())
      std::fprintf(stderr, "[kvaser] mhydra timeout waiting for cmd %u\n",
                   resp_cmd_no);
    return std::unexpected(error_code::read_timeout);
  }

  [[nodiscard]] result<> mhydra_map_channels() {
    std::memset(channel2he_, kvaser::ILLEGAL_HE, sizeof(channel2he_));
    std::memset(he2channel_, 0xFF, sizeof(he2channel_));

    for (uint8_t i = 0; i < channel_count_; ++i) {
      std::array<uint8_t, 32> cmd{};
      cmd[0] = kvaser::CMD_MAP_CHANNEL_REQ;
      kvaser::hydra_set_dst(cmd.data(), kvaser::ROUTER_HE);
      uint16_t tid = 0x40 | i;
      kvaser::hydra_set_seq(cmd.data(), tid);

      std::memcpy(&cmd[4], "CAN", 4);
      cmd[20] = i;

      std::array<uint8_t, 32> resp{};
      if (auto r = mhydra_send_and_wait(
              cmd.data(), kvaser::CMD_MAP_CHANNEL_RESP, resp.data(), 32);
          !r) {
        if (debug())
          std::fprintf(stderr, "[kvaser] MAP_CHANNEL failed for ch %u\n", i);
        return r;
      }

      uint8_t he = resp[4];
      channel2he_[i] = he;
      he2channel_[he] = i;

      if (debug())
        std::fprintf(stderr, "[kvaser] channel %u -> HE 0x%02X\n", i, he);
    }
    return {};
  }

  [[nodiscard]] result<> run_init_sequence_mhydra(slcan_bitrate bitrate) {
    if (debug()) std::fprintf(stderr, "[kvaser] === mhydra init start ===\n");

    channel_count_ = kvaser::HYDRA_MAX_CARD_CHANNELS;
    if (auto r = mhydra_map_channels(); !r) {
      if (debug()) std::fprintf(stderr, "[kvaser] channel mapping failed\n");
      return r;
    }

    {
      std::array<uint8_t, 32> cmd{};
      cmd[0] = kvaser::CMD_GET_CARD_INFO_REQ;
      kvaser::hydra_set_dst(cmd.data(), kvaser::ILLEGAL_HE);

      std::array<uint8_t, 32> resp{};
      if (auto r = mhydra_send_and_wait(
              cmd.data(), kvaser::CMD_GET_CARD_INFO_RESP, resp.data(), 32);
          !r) {
        if (debug()) std::fprintf(stderr, "[kvaser] get_card_info failed\n");
        return r;
      }

      uint32_t serial = static_cast<uint32_t>(resp[4]) |
                        (static_cast<uint32_t>(resp[5]) << 8) |
                        (static_cast<uint32_t>(resp[6]) << 16) |
                        (static_cast<uint32_t>(resp[7]) << 24);
      channel_count_ = resp[28];
      if (channel_count_ == 0) channel_count_ = 1;
      if (channel_count_ > kvaser::HYDRA_MAX_CARD_CHANNELS)
        channel_count_ = kvaser::HYDRA_MAX_CARD_CHANNELS;

      if (debug())
        std::fprintf(stderr, "[kvaser] mhydra channels=%u serial=%u\n",
                     channel_count_, serial);

      if (channel_ >= channel_count_) {
        if (debug())
          std::fprintf(stderr, "[kvaser] channel %u out of range (max %u)\n",
                       channel_, channel_count_ - 1);
        return std::unexpected(error_code::port_config_failed);
      }
    }

    {
      std::array<uint8_t, 32> cmd{};
      cmd[0] = kvaser::CMD_GET_SOFTWARE_INFO_REQ;
      kvaser::hydra_set_dst(cmd.data(), kvaser::ILLEGAL_HE);

      std::array<uint8_t, 32> resp{};
      if (auto r = mhydra_send_and_wait(
              cmd.data(), kvaser::CMD_GET_SOFTWARE_INFO_RESP, resp.data(), 32);
          !r) {
        if (debug())
          std::fprintf(stderr, "[kvaser] get_software_info failed\n");
        return r;
      }

      max_outstanding_tx_ = static_cast<uint16_t>(resp[12]) |
                            (static_cast<uint16_t>(resp[13]) << 8);
      if (debug())
        std::fprintf(stderr, "[kvaser] max_outstanding_tx=%u\n",
                     max_outstanding_tx_);
    }

    {
      std::array<uint8_t, 32> cmd{};
      cmd[0] = kvaser::CMD_GET_SOFTWARE_DETAILS_REQ;
      kvaser::hydra_set_dst(cmd.data(), kvaser::ILLEGAL_HE);
      cmd[4] = 1;

      std::array<uint8_t, 32> resp{};
      if (auto r = mhydra_send_and_wait(cmd.data(),
                                        kvaser::CMD_GET_SOFTWARE_DETAILS_RESP,
                                        resp.data(), 32);
          !r) {
        if (debug())
          std::fprintf(stderr, "[kvaser] get_software_details failed\n");
        return r;
      }

      uint32_t sw_options = static_cast<uint32_t>(resp[4]) |
                            (static_cast<uint32_t>(resp[5]) << 8) |
                            (static_cast<uint32_t>(resp[6]) << 16) |
                            (static_cast<uint32_t>(resp[7]) << 24);
      uint32_t sw_version = static_cast<uint32_t>(resp[8]) |
                            (static_cast<uint32_t>(resp[9]) << 8) |
                            (static_cast<uint32_t>(resp[10]) << 16) |
                            (static_cast<uint32_t>(resp[11]) << 24);

      use_hydra_ext_ = (sw_options & kvaser::SWOPTION_USE_HYDRA_EXT) != 0;

      if ((sw_options & kvaser::SWOPTION_CAN_CLK_MASK) ==
          kvaser::SWOPTION_80_MHZ_CAN_CLK) {
        can_clock_mhz_ = 80;
      } else if ((sw_options & kvaser::SWOPTION_CAN_CLK_MASK) ==
                 kvaser::SWOPTION_24_MHZ_CAN_CLK) {
        can_clock_mhz_ = 24;
      } else {
        can_clock_mhz_ = 80;
      }

      if (debug())
        std::fprintf(stderr,
                     "[kvaser] sw_version=0x%08X sw_options=0x%08X "
                     "hydra_ext=%d can_clk=%uMHz\n",
                     sw_version, sw_options, use_hydra_ext_ ? 1 : 0,
                     can_clock_mhz_);
    }

    {
      std::array<uint8_t, 32> cmd{};
      cmd[0] = kvaser::CMD_SET_DRIVERMODE_REQ;
      kvaser::hydra_set_dst(cmd.data(), channel2he_[channel_]);
      cmd[4] = kvaser::DRIVERMODE_NORMAL;

      if (auto r = mhydra_send_cmd(cmd.data()); !r) return r;
      if (debug())
        std::fprintf(stderr, "[kvaser] mhydra driver mode set to normal\n");
    }

    {
      static constexpr uint32_t bitrate_bps_map[] = {
          10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000,
      };
      uint32_t br = bitrate_bps_map[static_cast<unsigned>(bitrate) %
                                    (sizeof(bitrate_bps_map) /
                                     sizeof(bitrate_bps_map[0]))];

      uint8_t tseg1, tseg2, sjw;
      compute_mhydra_timing(br, tseg1, tseg2, sjw);

      std::array<uint8_t, 32> cmd{};
      cmd[0] = kvaser::CMD_SET_BUSPARAMS_REQ;
      kvaser::hydra_set_dst(cmd.data(), channel2he_[channel_]);

      cmd[4] = static_cast<uint8_t>(br);
      cmd[5] = static_cast<uint8_t>(br >> 8);
      cmd[6] = static_cast<uint8_t>(br >> 16);
      cmd[7] = static_cast<uint8_t>(br >> 24);
      cmd[8] = tseg1;
      cmd[9] = tseg2;
      cmd[10] = sjw;
      cmd[11] = 1;

      std::array<uint8_t, 32> resp{};
      if (auto r = mhydra_send_and_wait(
              cmd.data(), kvaser::CMD_SET_BUSPARAMS_RESP, resp.data(), 32);
          !r) {
        if (debug())
          std::fprintf(stderr, "[kvaser] mhydra set_busparams failed\n");
        return r;
      }
      if (debug())
        std::fprintf(stderr,
                     "[kvaser] mhydra busparams: bitrate=%u tseg1=%u tseg2=%u "
                     "sjw=%u (clk=%uMHz)\n",
                     br, tseg1, tseg2, sjw, can_clock_mhz_);
    }

    if (auto r = mhydra_cmd_start_chip(); !r) {
      if (debug()) std::fprintf(stderr, "[kvaser] mhydra start_chip failed\n");
      return r;
    }

    if (debug())
      std::fprintf(stderr, "[kvaser] === mhydra init complete ===\n");
    return {};
  }

  void compute_mhydra_timing(uint32_t bitrate, uint8_t& tseg1, uint8_t& tseg2,
                             uint8_t& sjw) {
    uint32_t clock_hz = can_clock_mhz_ * 1'000'000u;

    for (uint32_t tq = 25; tq >= 3; --tq) {
      uint32_t product = bitrate * tq;
      if (product == 0 || clock_hz % product != 0) continue;
      uint32_t brp = clock_hz / product;
      if (brp < 1 || brp > 8192) continue;

      uint32_t t1 = (tq * 80 / 100) - 1;
      uint32_t t2 = tq - 1 - t1;
      if (t1 < 1 || t1 > 255 || t2 < 1 || t2 > 127) continue;

      tseg1 = static_cast<uint8_t>(t1);
      tseg2 = static_cast<uint8_t>(t2);
      sjw = static_cast<uint8_t>(std::min(t2, 4u));
      return;
    }

    tseg1 = 15;
    tseg2 = 4;
    sjw = 4;
  }

  [[nodiscard]] result<> mhydra_cmd_start_chip() {
    std::array<uint8_t, 32> cmd{};
    cmd[0] = kvaser::CMD_START_CHIP_REQ;
    kvaser::hydra_set_dst(cmd.data(), channel2he_[channel_]);

    std::array<uint8_t, 32> resp{};
    return mhydra_send_and_wait(cmd.data(), kvaser::CMD_CHIP_STATE_EVENT,
                                resp.data(), 32);
  }

  [[nodiscard]] result<> mhydra_cmd_stop_chip() {
    std::array<uint8_t, 32> cmd{};
    cmd[0] = kvaser::CMD_STOP_CHIP_REQ;
    kvaser::hydra_set_dst(cmd.data(), channel2he_[channel_]);

    std::array<uint8_t, 32> resp{};
    return mhydra_send_and_wait(cmd.data(), kvaser::CMD_STOP_CHIP_RESP,
                                resp.data(), 32);
  }

  [[nodiscard]] result<> send_mhydra(const can_frame& frame) {
    if (use_hydra_ext_) {
      return send_mhydra_ext(frame);
    }

    std::array<uint8_t, 32> cmd{};
    cmd[0] = kvaser::CMD_TX_CAN_MESSAGE;
    kvaser::hydra_set_dst(cmd.data(), channel2he_[channel_]);
    uint8_t tid = next_trans_id();
    kvaser::hydra_set_seq(cmd.data(), tid);

    uint32_t id = frame.id;
    if (frame.extended) id |= 0x80000000u;
    cmd[4] = static_cast<uint8_t>(id);
    cmd[5] = static_cast<uint8_t>(id >> 8);
    cmd[6] = static_cast<uint8_t>(id >> 16);
    cmd[7] = static_cast<uint8_t>(id >> 24);

    uint8_t len = std::min(frame.dlc, static_cast<uint8_t>(8));
    std::memcpy(&cmd[8], frame.data.data(), len);

    cmd[16] = frame.dlc & 0x0F;
    uint8_t flags = 0;
    if (frame.rtr) flags |= kvaser::MSGFLAG_REMOTE_FRAME;
    cmd[17] = flags;
    cmd[18] = tid;
    cmd[19] = 0;
    cmd[20] = channel_;

    return mhydra_send_cmd(cmd.data());
  }

  [[nodiscard]] result<> send_mhydra_ext(const can_frame& frame) {
    uint8_t dlc = frame.dlc & 0x0F;
    uint8_t nbr_of_bytes = std::min(dlc, static_cast<uint8_t>(8));

    size_t total = 8 + 24 + nbr_of_bytes;
    total = (total + 7) & ~static_cast<size_t>(7);

    std::array<uint8_t, 128> cmd{};
    cmd[0] = kvaser::CMD_EXTENDED;
    kvaser::hydra_set_dst(cmd.data(), channel2he_[channel_]);
    uint8_t tid = next_trans_id();
    kvaser::hydra_set_seq(cmd.data(), tid);

    cmd[4] = static_cast<uint8_t>(total);
    cmd[5] = static_cast<uint8_t>(total >> 8);
    cmd[6] = kvaser::CMD_TX_CAN_MESSAGE_FD;
    cmd[7] = 0;

    uint32_t flags = 0;
    if (frame.rtr) flags |= kvaser::MSGFLAG_REMOTE_FRAME;
    cmd[8] = static_cast<uint8_t>(flags);
    cmd[9] = static_cast<uint8_t>(flags >> 8);
    cmd[10] = static_cast<uint8_t>(flags >> 16);
    cmd[11] = static_cast<uint8_t>(flags >> 24);

    uint32_t id = frame.id;
    if (frame.extended) id |= 0x80000000u;
    cmd[12] = static_cast<uint8_t>(id);
    cmd[13] = static_cast<uint8_t>(id >> 8);
    cmd[14] = static_cast<uint8_t>(id >> 16);
    cmd[15] = static_cast<uint8_t>(id >> 24);

    uint32_t fid = kvaser::fpga_id(frame.id, frame.extended, frame.rtr);
    cmd[16] = static_cast<uint8_t>(fid);
    cmd[17] = static_cast<uint8_t>(fid >> 8);
    cmd[18] = static_cast<uint8_t>(fid >> 16);
    cmd[19] = static_cast<uint8_t>(fid >> 24);

    uint32_t fctl = kvaser::fpga_control(dlc, true);
    cmd[20] = static_cast<uint8_t>(fctl);
    cmd[21] = static_cast<uint8_t>(fctl >> 8);
    cmd[22] = static_cast<uint8_t>(fctl >> 16);
    cmd[23] = static_cast<uint8_t>(fctl >> 24);

    cmd[24] = nbr_of_bytes;
    cmd[25] = dlc;

    std::memcpy(&cmd[32], frame.data.data(), nbr_of_bytes);

    int transferred = 0;
    int r = libusb_bulk_transfer(dev_, ep_bulk_out_, cmd.data(),
                                 static_cast<int>(total), &transferred,
                                 kvaser::k_cmd_timeout_ms);
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[kvaser] mhydra TX_FD failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
      return std::unexpected(error_code::write_error);
    }
    return {};
  }

  [[nodiscard]] result<std::vector<can_frame>> recv_many_mhydra(
      unsigned timeout_ms) {
    std::vector<can_frame> frames;
    std::array<uint8_t, 4096> buf{};

    int transferred = 0;
    int r = libusb_bulk_transfer(dev_, ep_cmd_in_, buf.data(),
                                 static_cast<int>(buf.size()), &transferred,
                                 static_cast<unsigned>(timeout_ms));

    if (r == LIBUSB_ERROR_TIMEOUT) return frames;
    if (r < 0) {
      if (debug())
        std::fprintf(stderr, "[kvaser] mhydra RX failed: %s\n",
                     libusb_strerror(static_cast<libusb_error>(r)));
      return std::unexpected(error_code::read_error);
    }

    size_t pos = 0;
    size_t total = static_cast<size_t>(transferred);

    while (pos + 4 <= total) {
      uint8_t cmd_no = buf[pos];
      size_t cmd_sz;

      if (cmd_no == kvaser::CMD_EXTENDED && pos + 6 <= total) {
        uint16_t ext_len = static_cast<uint16_t>(buf[pos + 4]) |
                           (static_cast<uint16_t>(buf[pos + 5]) << 8);
        cmd_sz = ext_len;
        if (cmd_sz < 8) cmd_sz = 8;
      } else if (cmd_no == 0) {
        pos += 4;
        continue;
      } else {
        cmd_sz = kvaser::HYDRA_CMD_SIZE;
      }

      if (pos + cmd_sz > total) break;

      if (cmd_no == kvaser::CMD_EXTENDED) {
        uint8_t ext_cmd = buf[pos + 6];
        if (ext_cmd == kvaser::CMD_RX_MESSAGE_FD) {
          mhydra_parse_rx_fd(&buf[pos], cmd_sz, frames);
        } else if (ext_cmd == kvaser::CMD_TX_ACKNOWLEDGE_FD) {
        } else {
          if (debug())
            std::fprintf(stderr, "[kvaser] mhydra ext cmd %u len %zu\n",
                         ext_cmd, cmd_sz);
        }
      } else if (cmd_no == kvaser::CMD_TX_ACKNOWLEDGE) {
      } else if (cmd_no == kvaser::CMD_CHIP_STATE_EVENT) {
        if (debug()) std::fprintf(stderr, "[kvaser] mhydra chip state event\n");
      } else if (cmd_no == kvaser::CMD_ERROR_EVENT) {
        if (debug()) std::fprintf(stderr, "[kvaser] mhydra error event\n");
      } else if (cmd_no == kvaser::CMD_LOG_MESSAGE) {
        mhydra_parse_rx_log(&buf[pos], frames);
      } else if (cmd_no == kvaser::CMD_START_CHIP_RESP ||
                 cmd_no == kvaser::CMD_STOP_CHIP_RESP ||
                 cmd_no == kvaser::CMD_MAP_CHANNEL_RESP ||
                 cmd_no == kvaser::CMD_GET_SOFTWARE_DETAILS_RESP ||
                 cmd_no == kvaser::CMD_SET_BUSPARAMS_RESP) {
      } else {
        if (debug()) std::fprintf(stderr, "[kvaser] mhydra cmd %u\n", cmd_no);
      }

      pos += cmd_sz;
    }

    return frames;
  }

  void mhydra_parse_rx_fd(const uint8_t* data, size_t len,
                          std::vector<can_frame>& out) {
    uint8_t src_he = kvaser::hydra_get_src(data);
    uint8_t chan = he2channel_[src_he];
    if (chan != channel_) return;

    if (len < 32) return;

    uint32_t flags = static_cast<uint32_t>(data[8]) |
                     (static_cast<uint32_t>(data[9]) << 8) |
                     (static_cast<uint32_t>(data[10]) << 16) |
                     (static_cast<uint32_t>(data[11]) << 24);

    if (flags & kvaser::MSGFLAG_ERROR_FRAME) return;
    if (flags & kvaser::MSGFLAG_TX) return;

    uint32_t id = static_cast<uint32_t>(data[12]) |
                  (static_cast<uint32_t>(data[13]) << 8) |
                  (static_cast<uint32_t>(data[14]) << 16) |
                  (static_cast<uint32_t>(data[15]) << 24);

    uint32_t fpga_control = static_cast<uint32_t>(data[20]) |
                            (static_cast<uint32_t>(data[21]) << 8) |
                            (static_cast<uint32_t>(data[22]) << 16) |
                            (static_cast<uint32_t>(data[23]) << 24);

    can_frame f{};
    f.timestamp = can_frame::clock::now();

    f.extended = (flags & kvaser::MSGFLAG_EXTENDED_ID) != 0;
    f.id = id & 0x1FFFFFFFu;
    f.rtr = (flags & kvaser::MSGFLAG_REMOTE_FRAME) != 0;

    uint8_t dlc = static_cast<uint8_t>((fpga_control >> 8) & 0xF);
    f.dlc = dlc;

    uint8_t nbr_of_bytes = std::min(dlc, static_cast<uint8_t>(8));
    if (f.rtr) nbr_of_bytes = 0;

    if (32 + nbr_of_bytes <= len) {
      std::memcpy(f.data.data(), &data[32], nbr_of_bytes);
    }

    if (debug()) {
      std::fprintf(stderr, "[kvaser] mhydra RX: id=0x%X%s dlc=%u", f.id,
                   f.extended ? "x" : "", f.dlc);
      for (uint8_t i = 0; i < nbr_of_bytes; ++i)
        std::fprintf(stderr, " %02X", f.data[i]);
      std::fprintf(stderr, "\n");
    }

    out.push_back(f);
  }

  void mhydra_parse_rx_log(const uint8_t* data, std::vector<can_frame>& out) {
    uint8_t src_he = kvaser::hydra_get_src(data);
    uint8_t chan = he2channel_[src_he];
    if (chan != channel_) return;

    uint32_t id = static_cast<uint32_t>(data[4]) |
                  (static_cast<uint32_t>(data[5]) << 8) |
                  (static_cast<uint32_t>(data[6]) << 16) |
                  (static_cast<uint32_t>(data[7]) << 24);

    uint8_t dlc = data[16];
    uint8_t flags = data[17];

    if (flags & kvaser::MSGFLAG_ERROR_FRAME) return;

    can_frame f{};
    f.timestamp = can_frame::clock::now();
    f.extended = (id & 0x80000000u) != 0;
    f.id = id & 0x1FFFFFFFu;
    f.dlc = dlc & 0x0F;
    f.rtr = (flags & kvaser::MSGFLAG_REMOTE_FRAME) != 0;

    uint8_t payload = std::min(f.dlc, static_cast<uint8_t>(8));
    std::memcpy(f.data.data(), &data[8], payload);

    if (debug()) {
      std::fprintf(stderr, "[kvaser] mhydra LOG_RX: id=0x%X%s dlc=%u", f.id,
                   f.extended ? "x" : "", f.dlc);
      for (uint8_t i = 0; i < payload; ++i)
        std::fprintf(stderr, " %02X", f.data[i]);
      std::fprintf(stderr, "\n");
    }

    out.push_back(f);
  }
};

}  // namespace jcan
