#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <expected>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace jcan::motec {

struct ld_event {
  std::string name;
  std::string session;
  std::string comment;
};

struct ld_venue {
  std::string name;
};

struct ld_vehicle {
  std::string id;
  uint32_t weight{};
  std::string type;
  std::string comment;
};

struct ld_channel {
  std::string name;
  std::string short_name;
  std::string unit;
  uint16_t freq_hz{};
  int16_t shift{};
  int16_t multiplier{};
  int16_t scale{};
  int16_t dec_places{};
  std::vector<double> samples;
};

struct ld_file {
  std::string date;
  std::string time_str;
  std::string driver;
  std::string vehicle_id;
  std::string venue_name;
  std::string short_comment;

  ld_event event;
  ld_venue venue;
  ld_vehicle vehicle;

  std::vector<ld_channel> channels;

  [[nodiscard]] double duration_seconds() const {
    double max_dur = 0.0;
    for (const auto& ch : channels) {
      if (ch.freq_hz > 0 && !ch.samples.empty()) {
        double dur = static_cast<double>(ch.samples.size()) /
                     static_cast<double>(ch.freq_hz);
        max_dur = std::max(max_dur, dur);
      }
    }
    return max_dur;
  }
};

namespace detail {

template <typename T>
[[nodiscard]] inline T read_le(const uint8_t* buf, std::size_t offset) {
  T val{};
  std::memcpy(&val, buf + offset, sizeof(T));
  return val;
}

[[nodiscard]] inline std::string read_string(const uint8_t* buf,
                                             std::size_t offset,
                                             std::size_t max_len) {
  const char* p = reinterpret_cast<const char*>(buf + offset);
  std::size_t len = 0;
  while (len < max_len && p[len] != '\0') ++len;
  std::string s(p, len);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\0')) s.pop_back();
  return s;
}

constexpr std::size_t k_head_size = 1762;

constexpr std::size_t off_magic = 0;
constexpr std::size_t off_meta_ptr = 8;
constexpr std::size_t off_data_ptr = 12;
constexpr std::size_t off_event_ptr = 36;
constexpr std::size_t off_device_serial = 70;
constexpr std::size_t off_device_type = 74;
constexpr std::size_t off_device_ver = 82;
constexpr std::size_t off_num_channs = 86;
constexpr std::size_t off_date = 94;
constexpr std::size_t off_time = 126;
constexpr std::size_t off_driver = 158;
constexpr std::size_t off_vehicleid = 222;
constexpr std::size_t off_venue = 350;
constexpr std::size_t off_short_comment = 1572;

constexpr std::size_t k_chan_size = 124;

constexpr std::size_t ch_off_prev = 0;
constexpr std::size_t ch_off_next = 4;
constexpr std::size_t ch_off_data_ptr = 8;
constexpr std::size_t ch_off_data_len = 12;
constexpr std::size_t ch_off_counter = 16;
constexpr std::size_t ch_off_dtype_a = 18;
constexpr std::size_t ch_off_dtype = 20;
constexpr std::size_t ch_off_freq = 22;
constexpr std::size_t ch_off_shift = 24;
constexpr std::size_t ch_off_mul = 26;
constexpr std::size_t ch_off_scale = 28;
constexpr std::size_t ch_off_dec = 30;
constexpr std::size_t ch_off_name = 32;
constexpr std::size_t ch_off_short = 64;
constexpr std::size_t ch_off_unit = 72;

constexpr std::size_t k_event_size = 1154;
constexpr std::size_t ev_off_name = 0;
constexpr std::size_t ev_off_session = 64;
constexpr std::size_t ev_off_comment = 128;
constexpr std::size_t ev_off_venue_ptr = 1152;

constexpr std::size_t k_venue_size = 1100;
constexpr std::size_t vn_off_name = 0;
constexpr std::size_t vn_off_vehicle_ptr = 1098;

constexpr std::size_t k_vehicle_size = 260;
constexpr std::size_t vh_off_id = 0;
constexpr std::size_t vh_off_weight = 192;
constexpr std::size_t vh_off_type = 196;
constexpr std::size_t vh_off_comment = 228;

[[nodiscard]] inline bool in_bounds(std::size_t buf_size, std::size_t offset,
                                    std::size_t need) {
  return offset <= buf_size && (buf_size - offset) >= need;
}

[[nodiscard]] inline std::size_t element_size(uint16_t dtype_a,
                                              uint16_t dtype) {
  if (dtype == 2) return 2;
  if (dtype == 4) return 4;
  return 0;
}

[[nodiscard]] inline bool is_float_type(uint16_t dtype_a) {
  return dtype_a == 0x07;
}

[[nodiscard]] inline double decode_sample(const uint8_t* buf,
                                          std::size_t offset, uint16_t dtype_a,
                                          uint16_t dtype) {
  if (dtype_a == 0x07) {
    if (dtype == 4) {
      float v;
      std::memcpy(&v, buf + offset, 4);
      return static_cast<double>(v);
    }
    if (dtype == 2) {
      uint16_t h;
      std::memcpy(&h, buf + offset, 2);
      uint32_t sign = (h >> 15) & 0x1;
      uint32_t exp = (h >> 10) & 0x1F;
      uint32_t mant = h & 0x3FF;
      float result;
      if (exp == 0) {
        result = std::ldexp(static_cast<float>(mant), -24);
      } else if (exp == 0x1F) {
        result = (mant == 0) ? std::numeric_limits<float>::infinity()
                             : std::numeric_limits<float>::quiet_NaN();
      } else {
        result = std::ldexp(static_cast<float>(mant + 1024),
                            static_cast<int>(exp) - 25);
      }
      return static_cast<double>(sign ? -result : result);
    }
  } else {
    if (dtype == 4) {
      int32_t v;
      std::memcpy(&v, buf + offset, 4);
      return static_cast<double>(v);
    }
    if (dtype == 2) {
      int16_t v;
      std::memcpy(&v, buf + offset, 2);
      return static_cast<double>(v);
    }
  }
  return 0.0;
}

}  // namespace detail

[[nodiscard]] inline std::expected<ld_file, std::string> load_ld(
    const std::filesystem::path& path) {
  using namespace detail;

  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) return std::unexpected("cannot open file: " + path.string());

  auto file_size = static_cast<std::size_t>(f.tellg());
  if (file_size < k_head_size)
    return std::unexpected("file too small for ld header");

  std::vector<uint8_t> buf(file_size);
  f.seekg(0);
  f.read(reinterpret_cast<char*>(buf.data()),
         static_cast<std::streamsize>(file_size));
  if (!f) return std::unexpected("failed to read file");

  const uint8_t* d = buf.data();

  uint32_t magic = read_le<uint32_t>(d, off_magic);
  if (magic != 0x40)
    return std::unexpected("bad ld magic: expected 0x40, got 0x" +
                           std::to_string(magic));

  ld_file ld;

  uint32_t meta_ptr = read_le<uint32_t>(d, off_meta_ptr);
  uint32_t event_ptr = read_le<uint32_t>(d, off_event_ptr);

  ld.date = read_string(d, off_date, 16);
  ld.time_str = read_string(d, off_time, 16);
  ld.driver = read_string(d, off_driver, 64);
  ld.vehicle_id = read_string(d, off_vehicleid, 64);
  ld.venue_name = read_string(d, off_venue, 64);
  ld.short_comment = read_string(d, off_short_comment, 64);

  if (event_ptr > 0 && in_bounds(file_size, event_ptr, k_event_size)) {
    ld.event.name = read_string(d, event_ptr + ev_off_name, 64);
    ld.event.session = read_string(d, event_ptr + ev_off_session, 64);
    ld.event.comment = read_string(d, event_ptr + ev_off_comment, 1024);

    uint16_t venue_ptr = read_le<uint16_t>(d, event_ptr + ev_off_venue_ptr);
    if (venue_ptr > 0 && in_bounds(file_size, venue_ptr, k_venue_size)) {
      ld.venue.name = read_string(d, venue_ptr + vn_off_name, 64);

      uint16_t vehicle_ptr =
          read_le<uint16_t>(d, venue_ptr + vn_off_vehicle_ptr);
      if (vehicle_ptr > 0 &&
          in_bounds(file_size, vehicle_ptr, k_vehicle_size)) {
        ld.vehicle.id = read_string(d, vehicle_ptr + vh_off_id, 64);
        ld.vehicle.weight = read_le<uint32_t>(d, vehicle_ptr + vh_off_weight);
        ld.vehicle.type = read_string(d, vehicle_ptr + vh_off_type, 32);
        ld.vehicle.comment = read_string(d, vehicle_ptr + vh_off_comment, 32);
      }
    }
  }

  std::unordered_set<std::string> seen_names;
  uint32_t chan_ptr = meta_ptr;
  std::size_t max_channels = 1024;

  while (chan_ptr > 0 && max_channels-- > 0) {
    if (!in_bounds(file_size, chan_ptr, k_chan_size)) break;

    ld_channel ch;

    uint32_t next_ptr = read_le<uint32_t>(d, chan_ptr + ch_off_next);
    uint32_t data_ptr = read_le<uint32_t>(d, chan_ptr + ch_off_data_ptr);
    uint32_t data_len = read_le<uint32_t>(d, chan_ptr + ch_off_data_len);
    uint16_t dtype_a = read_le<uint16_t>(d, chan_ptr + ch_off_dtype_a);
    uint16_t dtype = read_le<uint16_t>(d, chan_ptr + ch_off_dtype);

    ch.freq_hz = read_le<uint16_t>(d, chan_ptr + ch_off_freq);
    ch.shift = read_le<int16_t>(d, chan_ptr + ch_off_shift);
    ch.multiplier = read_le<int16_t>(d, chan_ptr + ch_off_mul);
    ch.scale = read_le<int16_t>(d, chan_ptr + ch_off_scale);
    ch.dec_places = read_le<int16_t>(d, chan_ptr + ch_off_dec);
    ch.name = read_string(d, chan_ptr + ch_off_name, 32);
    ch.short_name = read_string(d, chan_ptr + ch_off_short, 8);
    ch.unit = read_string(d, chan_ptr + ch_off_unit, 12);

    if (seen_names.contains(ch.name)) {
      int suffix = 2;
      while (seen_names.contains(ch.name + "_" + std::to_string(suffix)))
        ++suffix;
      ch.name += "_" + std::to_string(suffix);
    }
    seen_names.insert(ch.name);

    std::size_t elem_sz = element_size(dtype_a, dtype);
    if (elem_sz > 0 && data_len > 0 && ch.freq_hz > 0) {
      std::size_t byte_count = static_cast<std::size_t>(data_len) * elem_sz;
      if (in_bounds(file_size, data_ptr, byte_count)) {
        ch.samples.resize(data_len);

        double sc = (ch.scale != 0) ? static_cast<double>(ch.scale) : 1.0;
        double mul =
            (ch.multiplier != 0) ? static_cast<double>(ch.multiplier) : 1.0;
        double dec_factor = std::pow(10.0, static_cast<double>(-ch.dec_places));
        double shift = static_cast<double>(ch.shift);

        for (uint32_t i = 0; i < data_len; ++i) {
          double raw =
              decode_sample(d, data_ptr + static_cast<std::size_t>(i) * elem_sz,
                            dtype_a, dtype);
          ch.samples[i] = (raw / sc * dec_factor + shift) * mul;
        }
      }
    }

    ld.channels.push_back(std::move(ch));
    chan_ptr = next_ptr;
  }

  return ld;
}

}  // namespace jcan::motec
