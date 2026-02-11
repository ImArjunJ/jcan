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
  std::vector<std::string> dbc_paths;
  bool show_signals{true};
  bool show_transmitter{true};
  bool show_statistics{true};
  bool show_plotter{true};
  int window_width{1280};
  int window_height{800};
  float ui_scale{1.0f};
  std::string log_dir;  // auto-log directory; empty = default ~/jcan_logs/

  static std::filesystem::path default_log_dir() {
    const char* home = std::getenv("HOME");
    if (!home) home = "/tmp";
    return std::filesystem::path(home) / "jcan_logs";
  }

  std::filesystem::path effective_log_dir() const {
    if (!log_dir.empty()) return log_dir;
    return default_log_dir();
  }

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
    {
      std::string joined;
      for (std::size_t i = 0; i < dbc_paths.size(); ++i) {
        if (i) joined += ';';
        joined += dbc_paths[i];
      }
      ofs << "dbc_paths=" << joined << "\n";
    }
    ofs << "show_signals=" << (show_signals ? 1 : 0) << "\n";
    ofs << "show_transmitter=" << (show_transmitter ? 1 : 0) << "\n";
    ofs << "show_statistics=" << (show_statistics ? 1 : 0) << "\n";
    ofs << "show_plotter=" << (show_plotter ? 1 : 0) << "\n";
    ofs << "window_width=" << window_width << "\n";
    ofs << "window_height=" << window_height << "\n";
    ofs << "ui_scale=" << ui_scale << "\n";
    ofs << "log_dir=" << log_dir << "\n";

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
    {
      auto raw = get_str("dbc_paths");
      if (raw.empty()) raw = get_str("last_dbc_path");
      dbc_paths.clear();
      if (!raw.empty()) {
        std::size_t pos = 0;
        while (pos < raw.size()) {
          auto sep = raw.find(';', pos);
          if (sep == std::string::npos) sep = raw.size();
          auto p = raw.substr(pos, sep - pos);
          if (!p.empty()) dbc_paths.push_back(p);
          pos = sep + 1;
        }
      }
    }
    show_signals = get_int("show_signals", 1) != 0;
    show_transmitter = get_int("show_transmitter", 1) != 0;
    show_statistics = get_int("show_statistics", 1) != 0;
    show_plotter = get_int("show_plotter", 1) != 0;
    window_width = get_int("window_width", 1280);
    window_height = get_int("window_height", 800);
    {
      auto it = kv.find("ui_scale");
      if (it != kv.end()) {
        try {
          ui_scale = std::stof(it->second);
        } catch (...) {
        }
        if (ui_scale < 0.5f) ui_scale = 0.5f;
        if (ui_scale > 3.0f) ui_scale = 3.0f;
      }
    }
    log_dir = get_str("log_dir");

    return true;
  }
};

}  // namespace jcan
