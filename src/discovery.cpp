#include "discovery.hpp"

#include <libserialport.h>

#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace jcan {

[[nodiscard]] std::vector<device_descriptor> discover_adapters() {
  std::vector<device_descriptor> out;

  struct sp_port** port_list = nullptr;
  if (sp_list_ports(&port_list) == SP_OK && port_list) {
    for (int i = 0; port_list[i]; ++i) {
      struct sp_port* p = port_list[i];
      int vid = 0, pid = 0;
      if (sp_get_port_usb_vid_pid(p, &vid, &pid) == SP_OK) {
        if (vid == 0x0403 && pid == 0x6015) {
          device_descriptor d;
          d.kind = adapter_kind::serial_slcan;
          d.port = sp_get_port_name(p);
          const char* desc = sp_get_port_description(p);
          d.friendly_name = std::format("{} (FTDI {:04X}:{:04X})",
                                        desc ? desc : d.port, vid, pid);
          out.push_back(std::move(d));
        }
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
#endif

  out.push_back(device_descriptor{
      .kind = adapter_kind::mock,
      .port = "mock0",
      .friendly_name = "Virtual Mock Adapter",
  });

  return out;
}

}
