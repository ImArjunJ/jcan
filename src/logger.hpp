#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "types.hpp"

namespace jcan {

class frame_logger {
 public:
  enum class format_kind { csv, asc };

  bool recording() const { return recording_; }
  std::size_t frame_count() const { return frame_count_; }
  const std::string& filename() const { return filename_; }
  format_kind format() const { return format_; }

  bool start(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    if (ext == ".asc")
      return start_asc(path);
    else
      return start_csv(path);
  }

  bool start_csv(const std::filesystem::path& path) {
    ofs_.open(path, std::ios::out | std::ios::trunc);
    if (!ofs_.is_open()) return false;
    ofs_ << "timestamp_us,dir,id,extended,rtr,dlc,fd,brs,data\n";
    filename_ = path.filename().string();
    frame_count_ = 0;
    start_time_ = can_frame::clock::now();
    format_ = format_kind::csv;
    recording_ = true;
    return true;
  }

  bool start_asc(const std::filesystem::path& path) {
    ofs_.open(path, std::ios::out | std::ios::trunc);
    if (!ofs_.is_open()) return false;
    ofs_ << "date Thu Jan  1 00:00:00 AM 1970\n";
    ofs_ << "base hex  timestamps absolute\n";
    ofs_ << "internal events logged\n";
    ofs_ << "Begin TriggerBlock Thu Jan  1 00:00:00 AM 1970\n";
    filename_ = path.filename().string();
    frame_count_ = 0;
    start_time_ = can_frame::clock::now();
    format_ = format_kind::asc;
    recording_ = true;
    return true;
  }

  void log(const can_frame& f) {
    if (!recording_) return;
    if (format_ == format_kind::asc)
      log_asc(f);
    else
      log_csv(f);
  }

  void stop() {
    if (!recording_) return;
    if (format_ == format_kind::asc) {
      ofs_ << "End TriggerBlock\n";
    }
    ofs_.close();
    recording_ = false;
  }

  static std::vector<std::pair<int64_t, can_frame>> load_csv(
      const std::filesystem::path& path) {
    std::vector<std::pair<int64_t, can_frame>> out;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return out;
    std::string line;
    std::getline(ifs, line);
    while (std::getline(ifs, line)) {
      if (line.empty()) continue;
      auto entry = parse_csv_line(line);
      if (entry) out.push_back(*entry);
    }
    return out;
  }

  static std::vector<std::pair<int64_t, can_frame>> load_asc(
      const std::filesystem::path& path) {
    std::vector<std::pair<int64_t, can_frame>> out;
    std::ifstream ifs(path);
    if (!ifs.is_open()) return out;
    std::string line;
    while (std::getline(ifs, line)) {
      if (line.empty()) continue;
      auto entry = parse_asc_line(line);
      if (entry) out.push_back(*entry);
    }
    return out;
  }

  static bool export_to_file(const std::filesystem::path& path,
                             const std::vector<can_frame>& frames,
                             can_frame::clock::time_point base_time) {
    auto ext = path.extension().string();
    for (auto& c : ext) c = static_cast<char>(std::tolower(c));
    bool asc = (ext == ".asc");

    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open()) return false;

    if (asc) {
      ofs << "date Thu Jan  1 00:00:00 AM 1970\n";
      ofs << "base hex  timestamps absolute\n";
      ofs << "internal events logged\n";
      ofs << "Begin TriggerBlock Thu Jan  1 00:00:00 AM 1970\n";
    } else {
      ofs << "timestamp_us,dir,id,extended,rtr,dlc,fd,brs,data\n";
    }

    for (auto& f : frames) {
      auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                    f.timestamp - base_time)
                    .count();
      uint8_t len = frame_payload_len(f);

      if (asc) {
        double seconds = static_cast<double>(us) / 1e6;
        ofs << std::format("{:>12.6f}", seconds) << "  1  ";
        if (f.extended)
          ofs << std::format("{:08X}", f.id) << "x";
        else
          ofs << std::format("{:03X}", f.id);
        ofs << (f.tx ? "  Tx  " : "  Rx  ") << (f.fd ? "fd  " : "d  ")
            << static_cast<int>(len);
        for (uint8_t i = 0; i < len; ++i)
          ofs << std::format("  {:02X}", f.data[i]);
        if (f.fd && f.brs) ofs << "  BRS";
        ofs << "\n";
      } else {
        ofs << us << "," << (f.tx ? "Tx" : "Rx") << ",0x"
            << std::format("{:03X}", f.id) << "," << (f.extended ? 1 : 0) << ","
            << (f.rtr ? 1 : 0) << "," << static_cast<int>(f.dlc) << ","
            << (f.fd ? 1 : 0) << "," << (f.brs ? 1 : 0) << ",";
        for (uint8_t i = 0; i < len; ++i) {
          if (i) ofs << ' ';
          ofs << std::format("{:02X}", f.data[i]);
        }
        ofs << "\n";
      }
    }

    if (asc) ofs << "End TriggerBlock\n";
    return true;
  }

 private:
  void log_csv(const can_frame& f) {
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  f.timestamp - start_time_)
                  .count();
    uint8_t len = frame_payload_len(f);
    ofs_ << us << "," << (f.tx ? "Tx" : "Rx") << ",0x"
         << std::format("{:03X}", f.id) << "," << (f.extended ? 1 : 0) << ","
         << (f.rtr ? 1 : 0) << "," << static_cast<int>(f.dlc) << ","
         << (f.fd ? 1 : 0) << "," << (f.brs ? 1 : 0) << ",";
    for (uint8_t i = 0; i < len; ++i) {
      if (i) ofs_ << ' ';
      ofs_ << std::format("{:02X}", f.data[i]);
    }
    ofs_ << "\n";
    ++frame_count_;
  }

  void log_asc(const can_frame& f) {
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                  f.timestamp - start_time_)
                  .count();
    double seconds = static_cast<double>(us) / 1e6;
    uint8_t len = frame_payload_len(f);

    ofs_ << std::format("{:>12.6f}", seconds) << "  1  ";
    if (f.extended)
      ofs_ << std::format("{:08X}", f.id) << "x";
    else
      ofs_ << std::format("{:03X}", f.id);
    ofs_ << (f.tx ? "  Tx  " : "  Rx  ") << (f.fd ? "fd  " : "d  ")
         << static_cast<int>(len);
    for (uint8_t i = 0; i < len; ++i) {
      ofs_ << std::format("  {:02X}", f.data[i]);
    }
    if (f.fd && f.brs) ofs_ << "  BRS";
    ofs_ << "\n";
    ++frame_count_;
  }

  static std::optional<std::pair<int64_t, can_frame>> parse_csv_line(
      const std::string& line) {
    can_frame f{};
    int64_t ts_us = 0;
    std::istringstream ss(line);
    std::string tok;

    if (!std::getline(ss, tok, ',')) return std::nullopt;
    ts_us = std::stoll(tok);

    if (!std::getline(ss, tok, ',')) return std::nullopt;
    if (tok == "Tx" || tok == "Rx") {
      f.tx = (tok == "Tx");
      if (!std::getline(ss, tok, ',')) return std::nullopt;
    }
    f.id = static_cast<uint32_t>(std::stoul(tok, nullptr, 0));

    if (!std::getline(ss, tok, ',')) return std::nullopt;
    f.extended = (tok == "1");

    if (!std::getline(ss, tok, ',')) return std::nullopt;
    f.rtr = (tok == "1");

    if (!std::getline(ss, tok, ',')) return std::nullopt;
    f.dlc = static_cast<uint8_t>(std::stoi(tok));

    if (!std::getline(ss, tok, ',')) return std::nullopt;

    if (tok == "0" || tok == "1") {
      f.fd = (tok == "1");
      if (!std::getline(ss, tok, ',')) return std::nullopt;
      f.brs = (tok == "1");
      if (!std::getline(ss, tok)) return std::nullopt;
    }
    std::istringstream ds(tok);
    std::string byte_tok;
    uint8_t di = 0;
    uint8_t max_len = frame_payload_len(f);
    while (ds >> byte_tok && di < max_len) {
      f.data[di++] = static_cast<uint8_t>(std::stoul(byte_tok, nullptr, 16));
    }

    return std::pair{ts_us, f};
  }

  static std::optional<std::pair<int64_t, can_frame>> parse_asc_line(
      const std::string& line) {
    if (line.empty() || !std::isdigit(static_cast<unsigned char>(line[0])))
      return std::nullopt;

    std::istringstream ss(line);
    double timestamp_sec = 0;
    std::string channel, id_str, dir, d_marker;
    int dlc = 0;

    ss >> timestamp_sec >> channel >> id_str >> dir >> d_marker >> dlc;
    if (ss.fail()) return std::nullopt;

    can_frame f{};
    f.tx = (dir == "Tx");
    int64_t ts_us = static_cast<int64_t>(timestamp_sec * 1e6);

    if (!id_str.empty() && id_str.back() == 'x') {
      f.extended = true;
      id_str.pop_back();
    }
    f.id = static_cast<uint32_t>(std::stoul(id_str, nullptr, 16));
    f.dlc = static_cast<uint8_t>(std::clamp(dlc, 0, 8));

    for (uint8_t i = 0; i < f.dlc; ++i) {
      std::string byte_tok;
      if (!(ss >> byte_tok)) break;
      f.data[i] = static_cast<uint8_t>(std::stoul(byte_tok, nullptr, 16));
    }

    return std::pair{ts_us, f};
  }

  bool recording_{false};
  format_kind format_{format_kind::csv};
  std::ofstream ofs_;
  std::string filename_;
  std::size_t frame_count_{0};
  can_frame::clock::time_point start_time_;
};

}  // namespace jcan
