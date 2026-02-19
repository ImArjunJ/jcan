#include "discovery.hpp"

#include <libserialport.h>

#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

#if defined(JCAN_HAS_VECTOR) || defined(JCAN_HAS_KVASER)
#include <libusb.h>
#endif

#ifdef JCAN_HAS_KVASER
#include "hardware_kvaser.hpp"
#endif
#ifdef _WIN32
#include "hardware_kvaser_canlib.hpp"
#endif

namespace jcan {

struct known_usb_id {
  int vid;
  int pid;
  const char* label;
  bool slcan_capable;
};

static constexpr known_usb_id k_known_serial_can[] = {
    {0x0403, 0x6015, "CANdapter (FTDI FT240X)", true},
    {0x0403, 0x6001, "Lawicel CANUSB (FTDI)", true},
    {0x0403, 0x6014, "FTDI FT232H CAN", true},
    {0x0483, 0x5740, "CANable / STM32 CDC", true},
    {0x1D50, 0x606F, "CANtact", true},
    {0x16D0, 0x117E, "USBtin", true},
    {0x04D8, 0x000A, "Microchip CAN", true},
    {0x1CBE, 0x00FD, "TI TCAN", true},
};

struct known_usb_vendor {
  int vid;
  const char* vendor;
  const char* hint;
};

static constexpr known_usb_vendor k_known_can_vendors[] = {
    {0x0403, "FTDI", nullptr},
    {0x0483, "STMicro", nullptr},
    {0x1D50, "OpenMoko", nullptr},
#ifdef JCAN_HAS_VECTOR
    {0x1248, "Vector", nullptr},
#else
    {0x1248, "Vector", "rebuild jcan with libusb for native Vector support"},
#endif
    {0x0C72, "PEAK-System", "install peak linux driver (peak_usb module)"},
#ifdef JCAN_HAS_KVASER
    {0x0BFD, "Kvaser", nullptr},
#else
    {0x0BFD, "Kvaser", "run: sudo modprobe kvaser_usb"},
#endif
    {0x12D6, "EMS Wuensche", "install ems_usb kernel module"},
    {0x1CBE, "Texas Instruments", nullptr},
};

static const known_usb_id* find_known_serial(int vid, int pid) {
  for (const auto& e : k_known_serial_can)
    if (e.vid == vid && e.pid == pid) return &e;
  return nullptr;
}

static const known_usb_vendor* find_known_vendor(int vid) {
  for (const auto& e : k_known_can_vendors)
    if (e.vid == vid) return &e;
  return nullptr;
}

#ifdef JCAN_HAS_VECTOR
struct known_vector_device {
  int pid;
  const char* label;
  int num_channels;
};

static constexpr known_vector_device k_known_vector[] = {
    {0x1073, "VN1640A", 4},
    {0x1072, "VN1630A", 2},
    {0x1074, "VN1610", 2},
};

static const known_vector_device* find_known_vector(int pid) {
  for (const auto& e : k_known_vector)
    if (e.pid == pid) return &e;
  return nullptr;
}
#endif

[[nodiscard]] std::vector<device_descriptor> discover_adapters() {
  std::vector<device_descriptor> out;

  struct sp_port** port_list = nullptr;
  if (sp_list_ports(&port_list) == SP_OK && port_list) {
    for (int i = 0; port_list[i]; ++i) {
      struct sp_port* p = port_list[i];
      int vid = 0, pid = 0;
      if (sp_get_port_usb_vid_pid(p, &vid, &pid) != SP_OK) continue;
      if (auto* known = find_known_serial(vid, pid); known) {
        device_descriptor d;
        d.kind = adapter_kind::serial_slcan;
        d.port = sp_get_port_name(p);
        const char* desc = sp_get_port_description(p);
        d.friendly_name =
            std::format("{} ({})", desc ? desc : d.port, known->label);
        out.push_back(std::move(d));
      }
    }
    sp_free_port_list(port_list);
  }

#ifdef JCAN_HAS_SOCKETCAN
  namespace fs = std::filesystem;
  const fs::path net_class{"/sys/class/net"};
  if (fs::exists(net_class)) {
    for (const auto& entry : fs::directory_iterator(net_class)) {
      auto type_path = entry.path() / "type";
      if (!fs::exists(type_path)) continue;
      std::ifstream ifs(type_path);
      int type = 0;
      ifs >> type;
      if (type == 280) {
        device_descriptor d;
        d.kind = adapter_kind::socket_can;
        d.port = entry.path().filename().string();
        d.friendly_name = std::format("SocketCAN: {}", d.port);
        out.push_back(std::move(d));
      }
    }
  }

  const fs::path usb_devices{"/sys/bus/usb/devices"};
  if (fs::exists(usb_devices)) {
    for (const auto& entry : fs::directory_iterator(usb_devices)) {
      auto vid_path = entry.path() / "idVendor";
      auto pid_path = entry.path() / "idProduct";
      if (!fs::exists(vid_path) || !fs::exists(pid_path)) continue;

      std::string vid_str, pid_str;
      {
        std::ifstream f(vid_path);
        std::getline(f, vid_str);
      }
      {
        std::ifstream f(pid_path);
        std::getline(f, pid_str);
      }
      int vid = 0, pid = 0;
      try {
        vid = std::stoi(vid_str, nullptr, 16);
      } catch (...) {
        continue;
      }
      try {
        pid = std::stoi(pid_str, nullptr, 16);
      } catch (...) {
        continue;
      }

      auto* vendor = find_known_vendor(vid);
      if (!vendor) continue;

      if (find_known_serial(vid, pid)) continue;

      std::string usb_path = entry.path().filename().string();
      bool has_socketcan_for_device = false;
      for (const auto& net_entry : fs::directory_iterator(net_class)) {
        auto dev_link = net_entry.path() / "device";
        if (!fs::is_symlink(dev_link)) continue;
        auto target = fs::read_symlink(dev_link).string();
        if (target.find(usb_path) != std::string::npos) {
          has_socketcan_for_device = true;
          break;
        }
      }
      if (has_socketcan_for_device) continue;

#ifdef JCAN_HAS_VECTOR

      if (vid == 0x1248) {
        if (auto* vdev = find_known_vector(pid); vdev) {
          std::string product;
          auto prod_path = entry.path() / "product";
          if (fs::exists(prod_path)) {
            std::ifstream f(prod_path);
            std::getline(f, product);
          }
          if (product.empty()) product = vdev->label;

          std::string usb_path = entry.path().filename().string();
          for (int ch = 0; ch < vdev->num_channels; ++ch) {
            device_descriptor d;
            d.kind = adapter_kind::vector_xl;
            d.port = std::format("{}:{}", usb_path, ch);
            d.friendly_name = std::format("Vector {} CH{} ({:04X}:{:04X})",
                                          product, ch + 1, vid, pid);
            out.push_back(std::move(d));
          }
          continue;
        }
      }
#endif

#ifdef JCAN_HAS_KVASER
      if (vid == 0x0BFD) {
        std::string product;
        auto prod_path = entry.path() / "product";
        if (fs::exists(prod_path)) {
          std::ifstream f(prod_path);
          std::getline(f, product);
        }
        if (product.empty()) product = "Kvaser";

        uint8_t channels = 1;
        auto* kp = kvaser::find_any(static_cast<uint16_t>(pid));
        if (kp) {
          if (product.empty() || product == "Kvaser") product = kp->name;
          channels = kp->channels;
        }

        for (uint8_t ch = 0; ch < channels; ++ch) {
          device_descriptor d;
          d.kind = adapter_kind::kvaser_usb;
          d.port = std::format("{}:{}", pid, ch);
          d.friendly_name = std::format("Kvaser {} CH{} ({:04X}:{:04X})",
                                        product, ch + 1, vid, pid);
          out.push_back(std::move(d));
        }
        continue;
      }
#endif

      std::string product;
      auto prod_path = entry.path() / "product";
      if (fs::exists(prod_path)) {
        std::ifstream f(prod_path);
        std::getline(f, product);
      }
      if (product.empty()) product = vendor->vendor;

      device_descriptor d;
      d.kind = adapter_kind::unbound;
      d.port = entry.path().filename().string();
      d.friendly_name = std::format("{} ({:04X}:{:04X})", product, vid, pid);
      if (vendor->hint) d.friendly_name += std::format(" - {}", vendor->hint);
      out.push_back(std::move(d));
    }
  }
#endif

#ifdef _WIN32
  {
    auto canlib_channels = canlib::enumerate_channels();
    for (const auto& ch : canlib_channels) {
      device_descriptor d;
      d.kind = adapter_kind::kvaser_canlib;
      d.port = std::format("canlib:{}", ch.canlib_channel);
      d.friendly_name =
          std::format("{} CH{}", ch.device_name, ch.channel_on_card + 1);
      out.push_back(std::move(d));
    }
  }
#endif

#if !defined(JCAN_HAS_SOCKETCAN) && \
    (defined(JCAN_HAS_VECTOR) || defined(JCAN_HAS_KVASER))
  {
    libusb_context* usb_ctx = nullptr;
    if (libusb_init(&usb_ctx) == 0 && usb_ctx) {
      libusb_device** devlist = nullptr;
      ssize_t cnt = libusb_get_device_list(usb_ctx, &devlist);
      for (ssize_t i = 0; i < cnt; ++i) {
        struct libusb_device_descriptor udesc{};
        if (libusb_get_device_descriptor(devlist[i], &udesc) != 0) continue;

        int vid = udesc.idVendor;
        int pid = udesc.idProduct;

        if (find_known_serial(vid, pid)) continue;

        auto* vendor = find_known_vendor(vid);

#ifdef JCAN_HAS_VECTOR
        if (vid == 0x1248) {
          if (auto* vdev = find_known_vector(pid); vdev) {
            std::string product;
            if (udesc.iProduct) {
              libusb_device_handle* h = nullptr;
              if (libusb_open(devlist[i], &h) == 0 && h) {
                unsigned char buf[256]{};
                int len = libusb_get_string_descriptor_ascii(h, udesc.iProduct,
                                                             buf, sizeof(buf));
                if (len > 0)
                  product.assign(reinterpret_cast<char*>(buf),
                                 static_cast<size_t>(len));
                libusb_close(h);
              }
            }
            if (product.empty()) product = vdev->label;

            uint8_t bus = libusb_get_bus_number(devlist[i]);
            uint8_t addr = libusb_get_device_address(devlist[i]);
            std::string usb_path = std::format("{}-{}", bus, addr);

            for (int ch = 0; ch < vdev->num_channels; ++ch) {
              device_descriptor d;
              d.kind = adapter_kind::vector_xl;
              d.port = std::format("{}:{}", usb_path, ch);
              d.friendly_name = std::format("Vector {} CH{} ({:04X}:{:04X})",
                                            product, ch + 1, vid, pid);
              out.push_back(std::move(d));
            }
            continue;
          }
        }
#endif

#ifdef JCAN_HAS_KVASER
        if (vid == 0x0BFD) {
#ifdef _WIN32
          continue;
#endif
          std::string product;
          if (udesc.iProduct) {
            libusb_device_handle* h = nullptr;
            if (libusb_open(devlist[i], &h) == 0 && h) {
              unsigned char buf[256]{};
              int len = libusb_get_string_descriptor_ascii(h, udesc.iProduct,
                                                           buf, sizeof(buf));
              if (len > 0)
                product.assign(reinterpret_cast<char*>(buf),
                               static_cast<size_t>(len));
              libusb_close(h);
            }
          }
          if (product.empty()) product = "Kvaser";

          uint8_t channels = 1;
          auto* kp = kvaser::find_any(static_cast<uint16_t>(pid));
          if (kp) {
            if (product.empty() || product == "Kvaser") product = kp->name;
            channels = kp->channels;
          }

          for (uint8_t ch = 0; ch < channels; ++ch) {
            device_descriptor d;
            d.kind = adapter_kind::kvaser_usb;
            d.port = std::format("{}:{}", pid, ch);
            d.friendly_name = std::format("Kvaser {} CH{} ({:04X}:{:04X})",
                                          product, ch + 1, vid, pid);
            out.push_back(std::move(d));
          }
          continue;
        }
#endif

        if (vendor) {
          std::string product;
          if (udesc.iProduct) {
            libusb_device_handle* h = nullptr;
            if (libusb_open(devlist[i], &h) == 0 && h) {
              unsigned char buf[256]{};
              int len = libusb_get_string_descriptor_ascii(h, udesc.iProduct,
                                                           buf, sizeof(buf));
              if (len > 0)
                product.assign(reinterpret_cast<char*>(buf),
                               static_cast<size_t>(len));
              libusb_close(h);
            }
          }
          if (product.empty()) product = vendor->vendor;

          device_descriptor d;
          d.kind = adapter_kind::unbound;
          d.port = std::format("{:04X}:{:04X}", vid, pid);
          d.friendly_name =
              std::format("{} ({:04X}:{:04X})", product, vid, pid);
          if (vendor->hint)
            d.friendly_name += std::format(" - {}", vendor->hint);
          out.push_back(std::move(d));
        }
      }
      if (devlist) libusb_free_device_list(devlist, 1);
      libusb_exit(usb_ctx);
    }
  }
#endif

  out.push_back(device_descriptor{
      .kind = adapter_kind::mock,
      .port = "mock0",
      .friendly_name = "Virtual Mock Adapter",
  });

  out.push_back(device_descriptor{
      .kind = adapter_kind::mock_echo,
      .port = "echo0",
      .friendly_name = "Virtual Echo Adapter",
  });

  out.push_back(device_descriptor{
      .kind = adapter_kind::mock_fd,
      .port = "mockfd0",
      .friendly_name = "Virtual CAN-FD Adapter",
  });

  return out;
}

}  // namespace jcan
