#pragma once

#include "types.hpp"

#ifdef JCAN_HAS_SOCKETCAN

#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <optional>
#include <string>
#include <vector>

namespace jcan {

struct socket_can {
  int fd_{-1};
  bool open_{false};
  bool fd_enabled_{false};
  std::string iface_name_;

  static constexpr uint32_t bitrate_bps(slcan_bitrate br) {
    constexpr uint32_t map[] = {
        10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000,
    };
    auto idx = static_cast<unsigned>(br);
    return idx < sizeof(map) / sizeof(map[0]) ? map[idx] : 500000;
  }

  static bool iface_is_up(const std::string& name) {
    int s = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (s < 0) return false;
    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, name.c_str(), IFNAMSIZ - 1);
    bool up = false;
    if (::ioctl(s, SIOCGIFFLAGS, &ifr) == 0) up = (ifr.ifr_flags & IFF_UP) != 0;
    ::close(s);
    return up;
  }

  static int run_elevated(const std::string& cmd) {
    int rc = ::system(cmd.c_str());
    if (rc != 0) {
      auto sudo_cmd = "sudo " + cmd;
      rc = ::system(sudo_cmd.c_str());
    }
    return rc;
  }

  [[nodiscard]] result<> open(const std::string& iface_name,
                              slcan_bitrate bitrate = slcan_bitrate::s6,
                              [[maybe_unused]] unsigned baud = 0) {
    if (open_) return std::unexpected(error_code::already_open);

    if (!iface_is_up(iface_name)) {
      auto br = bitrate_bps(bitrate);
      auto cmd_down =
          std::format("ip link set {} down 2>/dev/null", iface_name);
      auto cmd_type = std::format(
          "ip link set {} type can bitrate {} 2>/dev/null", iface_name, br);
      auto cmd_up = std::format("ip link set {} up 2>/dev/null", iface_name);

      (void)run_elevated(cmd_down);
      if (run_elevated(cmd_type) != 0) {
        if (std::getenv("JCAN_DEBUG"))
          std::fprintf(stderr,
                       "[socketcan] set bitrate failed (may already be "
                       "configured or vcan)\n");
      }
      if (run_elevated(cmd_up) != 0) {
        if (std::getenv("JCAN_DEBUG"))
          std::fprintf(stderr, "[socketcan] ip link set up failed\n");
        return std::unexpected(error_code::port_config_failed);
      }
    } else if (std::getenv("JCAN_DEBUG")) {
      std::fprintf(stderr, "[socketcan] %s already UP, skipping config\n",
                   iface_name.c_str());
    }

    fd_ = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd_ < 0) return std::unexpected(error_code::socket_error);

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, iface_name.c_str(), IFNAMSIZ - 1);
    if (::ioctl(fd_, SIOCGIFINDEX, &ifr) < 0) {
      ::close(fd_);
      fd_ = -1;
      return std::unexpected(error_code::interface_not_found);
    }

    struct sockaddr_can addr{};
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (::bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) <
        0) {
      ::close(fd_);
      fd_ = -1;
      return std::unexpected(error_code::socket_error);
    }

    can_err_mask_t err_mask = CAN_ERR_MASK;
    ::setsockopt(fd_, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask,
                 sizeof(err_mask));

    int canfd_on = 1;
    if (::setsockopt(fd_, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &canfd_on,
                     sizeof(canfd_on)) == 0) {
      fd_enabled_ = true;
    }

    iface_name_ = iface_name;
    open_ = true;
    return {};
  }

  [[nodiscard]] result<> close() {
    if (!open_) return std::unexpected(error_code::not_open);
    ::close(fd_);
    fd_ = -1;
    open_ = false;

    if (!iface_name_.empty()) {
      auto cmd = std::format("ip link set {} down 2>/dev/null", iface_name_);
      (void)run_elevated(cmd);
      iface_name_.clear();
    }
    return {};
  }

  [[nodiscard]] result<> send(const can_frame& frame) {
    if (!open_) return std::unexpected(error_code::not_open);

    if (frame.fd && fd_enabled_) {
      struct canfd_frame cfd{};
      cfd.can_id = frame.id;
      if (frame.extended) cfd.can_id |= CAN_EFF_FLAG;
      if (frame.rtr) cfd.can_id |= CAN_RTR_FLAG;
      cfd.len = frame_payload_len(frame);
      cfd.flags = frame.brs ? CANFD_BRS : 0;
      std::memcpy(cfd.data, frame.data.data(), cfd.len);
      ssize_t n = ::write(fd_, &cfd, sizeof(cfd));
      if (n != sizeof(cfd)) return std::unexpected(error_code::write_error);
    } else {
      struct ::can_frame cf{};
      cf.can_id = frame.id;
      if (frame.extended) cf.can_id |= CAN_EFF_FLAG;
      if (frame.rtr) cf.can_id |= CAN_RTR_FLAG;
      cf.can_dlc = std::min(frame.dlc, static_cast<uint8_t>(8));
      std::memcpy(cf.data, frame.data.data(), cf.can_dlc);
      ssize_t n = ::write(fd_, &cf, sizeof(cf));
      if (n != sizeof(cf)) return std::unexpected(error_code::write_error);
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

    struct pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLIN;

    int ready = ::poll(&pfd, 1, static_cast<int>(timeout_ms));
    if (ready < 0) return std::unexpected(error_code::read_error);
    if (ready == 0) return frames;

    while (true) {
      if (fd_enabled_) {
        struct canfd_frame cfd{};
        ssize_t n = ::read(fd_, &cfd, sizeof(cfd));
        if (n == sizeof(struct canfd_frame)) {
          can_frame f{};
          f.timestamp = can_frame::clock::now();
          f.id = cfd.can_id & CAN_EFF_MASK;
          f.extended = (cfd.can_id & CAN_EFF_FLAG) != 0;
          f.rtr = (cfd.can_id & CAN_RTR_FLAG) != 0;
          f.error = (cfd.can_id & CAN_ERR_FLAG) != 0;
          f.fd = true;
          f.brs = (cfd.flags & CANFD_BRS) != 0;
          f.dlc = len_to_dlc(cfd.len);
          std::memcpy(f.data.data(), cfd.data, cfd.len);
          frames.push_back(f);
        } else if (n == sizeof(struct ::can_frame)) {
          auto* cf = reinterpret_cast<struct ::can_frame*>(&cfd);
          can_frame f{};
          f.timestamp = can_frame::clock::now();
          f.id = cf->can_id & CAN_EFF_MASK;
          f.extended = (cf->can_id & CAN_EFF_FLAG) != 0;
          f.rtr = (cf->can_id & CAN_RTR_FLAG) != 0;
          f.error = (cf->can_id & CAN_ERR_FLAG) != 0;
          f.dlc = cf->can_dlc;
          std::memcpy(f.data.data(), cf->data,
                      std::min(f.dlc, static_cast<uint8_t>(8)));
          frames.push_back(f);
        } else {
          break;
        }
      } else {
        struct ::can_frame cf{};
        ssize_t n = ::read(fd_, &cf, sizeof(cf));
        if (n != sizeof(cf)) break;

        can_frame f{};
        f.timestamp = can_frame::clock::now();
        f.id = cf.can_id & CAN_EFF_MASK;
        f.extended = (cf.can_id & CAN_EFF_FLAG) != 0;
        f.rtr = (cf.can_id & CAN_RTR_FLAG) != 0;
        f.error = (cf.can_id & CAN_ERR_FLAG) != 0;
        f.dlc = cf.can_dlc;
        std::memcpy(f.data.data(), cf.data,
                    std::min(f.dlc, static_cast<uint8_t>(8)));
        frames.push_back(f);
      }

      ready = ::poll(&pfd, 1, 0);
      if (ready <= 0) break;
    }

    return frames;
  }
};

}  // namespace jcan

#else

namespace jcan {

struct socket_can {
  [[nodiscard]] result<> open(const std::string&,
                              slcan_bitrate = slcan_bitrate::s6, unsigned = 0) {
    return std::unexpected(error_code::socket_error);
  }
  [[nodiscard]] result<> close() {
    return std::unexpected(error_code::not_open);
  }
  [[nodiscard]] result<> send(const can_frame&) {
    return std::unexpected(error_code::not_open);
  }
  [[nodiscard]] result<std::optional<can_frame>> recv(unsigned = 100) {
    return std::unexpected(error_code::not_open);
  }
  [[nodiscard]] result<std::vector<can_frame>> recv_many(unsigned = 100) {
    return std::unexpected(error_code::not_open);
  }
};

}  // namespace jcan

#endif
