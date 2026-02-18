#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef __linux__
#include <unistd.h>

#include <cerrno>
#endif

namespace jcan {

inline constexpr const char* k_udev_rule_path =
    "/etc/udev/rules.d/99-jcan-serial.rules";

inline constexpr const char* k_udev_rule_content =
    R"(SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", MODE="0666"
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", MODE="0666"
SUBSYSTEM=="tty", ATTRS{bInterfaceClass}=="02", MODE="0666"
)";

inline bool device_accessible(const std::string& path) {
#ifdef __linux__
  return ::access(path.c_str(), R_OK | W_OK) == 0;
#else
  (void)path;
  return true;
#endif
}

inline bool udev_rule_installed() {
  return std::filesystem::exists(k_udev_rule_path);
}

inline bool install_udev_rule() {
#ifdef __linux__
  auto tmp = std::filesystem::temp_directory_path() / "jcan-udev-rule.tmp";
  {
    std::ofstream ofs(tmp, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << k_udev_rule_content;
  }

  auto install_cmd = std::string("pkexec sh -c 'cp ") + tmp.string() + " " +
                     k_udev_rule_path + " && udevadm control --reload-rules" +
                     " && udevadm trigger --subsystem-match=tty'";
  int rc = std::system(install_cmd.c_str());

  std::filesystem::remove(tmp);

  return rc == 0;
#else
  return false;
#endif
}

inline bool retrigger_udev() {
#ifdef __linux__
  return std::system("udevadm trigger --subsystem-match=tty") == 0;
#else
  return false;
#endif
}

}  // namespace jcan
