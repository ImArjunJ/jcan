#pragma once

#include <cstddef>
#include <cstdint>

namespace jcan::vector::firmware {

extern "C" {
extern const uint8_t vn1640a_fw_data[];
extern const uint8_t vn1640a_fw_data_end[];
extern const uint8_t vn1640a_fpga_data[];
extern const uint8_t vn1640a_fpga_data_end[];
}

inline const uint8_t* main_fw() { return vn1640a_fw_data; }
inline size_t main_fw_size() {
  return static_cast<size_t>(vn1640a_fw_data_end - vn1640a_fw_data);
}

inline const uint8_t* fpga() { return vn1640a_fpga_data; }
inline size_t fpga_size() {
  return static_cast<size_t>(vn1640a_fpga_data_end - vn1640a_fpga_data);
}

}  // namespace jcan::vector::firmware
