#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

namespace jcan {

struct settings {
  int selected_bitrate{6};
  std::string last_adapter_port;
  std::string last_dbc_path;
  bool show_signal_watcher{true};
  bool show_transmitter{true};
  bool show_statistics{true};
  int window_width{1280};
  int window_height{800};

  static std::filesystem::path config_dir() {
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    std::filesystem::path dir;
    if (xdg && xdg[0] != '\0') {
      dir = std::filesystem::path(xdg) / "jcan";
    } else {
      const char* home = std::getenv("HOME");
      if (!home) home = "/tmp";
      dir = std::filesystem::path(home) / ".config" / "jcan";
    }
    return dir;
  }

  static std::filesystem::path config_path() {
    return config_dir() / "settings.ini";
  }

  bool save() const {
    auto dir = config_dir();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) return false;

    std::ofstream ofs(config_path(), std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return false;

    ofs << "selected_bitrate=" << selected_bitrate << "\n";
    ofs << "last_adapter_port=" << last_adapter_port << "\n";
    ofs << "last_dbc_path=" << last_dbc_path << "\n";
    ofs << "show_signal_watcher=" << (show_signal_watcher ? 1 : 0) << "\n";
    ofs << "show_transmitter=" << (show_transmitter ? 1 : 0) << "\n";
    ofs << "show_statistics=" << (show_statistics ? 1 : 0) << "\n";
    ofs << "window_width=" << window_width << "\n";
    ofs << "window_height=" << window_height << "\n";

    return true;
  }

  bool load() {
    std::ifstream ifs(config_path());
    if (!ifs.is_open()) return false;

    std::unordered_map<std::string, std::string> kv;
    std::string line;
    while (std::getline(ifs, line)) {
      if (line.empty() || line[0] == '#') continue;
      auto eq = line.find('=');
      if (eq == std::string::npos) continue;
      kv[line.substr(0, eq)] = line.substr(eq + 1);
    }

    auto get_int = [&](const std::string& key, int def) -> int {
      auto it = kv.find(key);
      if (it == kv.end()) return def;
      try {
        return std::stoi(it->second);
      } catch (...) {
        return def;
      }
    };

    auto get_str = [&](const std::string& key) -> std::string {
      auto it = kv.find(key);
      return (it != kv.end()) ? it->second : "";
    };

    selected_bitrate = get_int("selected_bitrate", 6);
    last_adapter_port = get_str("last_adapter_port");
    last_dbc_path = get_str("last_dbc_path");
    show_signal_watcher = get_int("show_signal_watcher", 1) != 0;
    show_transmitter = get_int("show_transmitter", 1) != 0;
    show_statistics = get_int("show_statistics", 1) != 0;
    window_width = get_int("window_width", 1280);
    window_height = get_int("window_height", 800);

    return true;
  }
};

}
