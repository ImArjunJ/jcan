#pragma once

#include <dbcppp/Network.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "types.hpp"

namespace jcan {

struct decoded_signal {
  std::string name;
  double value;
  std::string unit;
  double raw;
  double minimum;
  double maximum;
};

struct signal_info {
  std::string name;
  std::string unit;
  double factor;
  double offset;
  double minimum;
  double maximum;
  uint64_t start_bit;
  uint64_t bit_size;
  bool is_signed;
};

class dbc_engine {
 public:
  [[nodiscard]] bool loaded() const { return !networks_.empty(); }

  [[nodiscard]] std::vector<std::string> filenames() const {
    std::vector<std::string> out;
    out.reserve(networks_.size());
    for (const auto& n : networks_) out.push_back(n.filename);
    return out;
  }

  [[nodiscard]] const std::string& filename() const {
    static const std::string empty;
    if (networks_.empty()) return empty;
    return networks_.front().filename;
  }

  [[nodiscard]] std::vector<std::string> paths() const {
    std::vector<std::string> out;
    out.reserve(networks_.size());
    for (const auto& n : networks_) out.push_back(n.path);
    return out;
  }

  bool load(const std::filesystem::path& path) {
    for (const auto& n : networks_) {
      if (n.path == path.string()) return true;
    }
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    auto net = dbcppp::INetwork::LoadDBCFromIs(ifs);
    if (!net) return false;

    loaded_network ln;
    ln.net = std::move(net);
    ln.filename = path.filename().string();
    ln.path = path.string();
    networks_.push_back(std::move(ln));
    rebuild_index();
    return true;
  }

  void unload() {
    networks_.clear();
    msg_index_.clear();
  }

  void unload_one(const std::string& path) {
    auto it =
        std::remove_if(networks_.begin(), networks_.end(),
                       [&](const loaded_network& n) { return n.path == path; });
    if (it != networks_.end()) {
      networks_.erase(it, networks_.end());
      rebuild_index();
    }
  }

  [[nodiscard]] bool has_message(uint32_t id) const {
    return msg_index_.contains(static_cast<uint64_t>(id));
  }

  [[nodiscard]] std::string message_name(uint32_t id) const {
    auto it = msg_index_.find(static_cast<uint64_t>(id));
    if (it == msg_index_.end()) return {};
    return std::string(it->second->Name());
  }

  [[nodiscard]] uint8_t message_dlc(uint32_t id) const {
    auto it = msg_index_.find(static_cast<uint64_t>(id));
    if (it == msg_index_.end()) return 8;
    return static_cast<uint8_t>(
        std::min<uint64_t>(it->second->MessageSize(), 8));
  }

  [[nodiscard]] std::vector<signal_info> signal_infos(uint32_t id) const {
    std::vector<signal_info> out;
    auto it = msg_index_.find(static_cast<uint64_t>(id));
    if (it == msg_index_.end()) return out;

    for (const auto& sig : it->second->Signals()) {
      out.push_back(signal_info{
          .name = std::string(sig.Name()),
          .unit = std::string(sig.Unit()),
          .factor = sig.Factor(),
          .offset = sig.Offset(),
          .minimum = sig.Minimum(),
          .maximum = sig.Maximum(),
          .start_bit = sig.StartBit(),
          .bit_size = sig.BitSize(),
          .is_signed = sig.ValueType() == dbcppp::ISignal::EValueType::Signed,
      });
    }
    return out;
  }

  [[nodiscard]] std::vector<decoded_signal> decode(
      const can_frame& frame) const {
    std::vector<decoded_signal> out;
    auto it = msg_index_.find(static_cast<uint64_t>(frame.id));
    if (it == msg_index_.end()) return out;

    const auto* msg = it->second;
    const auto* mux_sig = msg->MuxSignal();

    for (const auto& sig : msg->Signals()) {
      if (sig.MultiplexerIndicator() ==
          dbcppp::ISignal::EMultiplexer::MuxValue) {
        if (mux_sig) {
          auto mux_val = mux_sig->Decode(frame.data.data());
          if (mux_val != sig.MultiplexerSwitchValue()) continue;
        }
      }

      double raw_val = static_cast<double>(sig.Decode(frame.data.data()));
      double phys = sig.RawToPhys(raw_val);

      out.push_back(decoded_signal{
          .name = std::string(sig.Name()),
          .value = phys,
          .unit = std::string(sig.Unit()),
          .raw = raw_val,
          .minimum = sig.Minimum(),
          .maximum = sig.Maximum(),
      });
    }
    return out;
  }

  [[nodiscard]] can_frame encode(
      uint32_t id,
      const std::unordered_map<std::string, double>& signal_values) const {
    can_frame f{};
    f.id = id;
    f.extended = (id > 0x7FF);
    std::memset(f.data.data(), 0, 64);

    auto it = msg_index_.find(static_cast<uint64_t>(id));
    if (it == msg_index_.end()) {
      f.dlc = 8;
      return f;
    }

    const auto* msg = it->second;
    f.dlc = static_cast<uint8_t>(std::min<uint64_t>(msg->MessageSize(), 8));

    for (const auto& sig : msg->Signals()) {
      auto sv = signal_values.find(std::string(sig.Name()));
      if (sv == signal_values.end()) continue;
      uint64_t raw_val = sig.PhysToRaw(sv->second);
      sig.Encode(raw_val, f.data.data());
    }
    f.timestamp = can_frame::clock::now();
    return f;
  }

  [[nodiscard]] std::vector<uint32_t> message_ids() const {
    std::vector<uint32_t> ids;
    ids.reserve(msg_index_.size());
    for (const auto& [id, _] : msg_index_)
      ids.push_back(static_cast<uint32_t>(id));
    std::sort(ids.begin(), ids.end());
    return ids;
  }

 private:
  struct loaded_network {
    std::unique_ptr<dbcppp::INetwork> net;
    std::string filename;
    std::string path;
  };

  void rebuild_index() {
    msg_index_.clear();
    for (const auto& ln : networks_) {
      if (!ln.net) continue;
      for (const auto& msg : ln.net->Messages()) {
        uint64_t id = msg.Id() & 0x1FFFFFFF;
        msg_index_[id] = &msg;
      }
    }
  }

  std::vector<loaded_network> networks_;
  std::unordered_map<uint64_t, const dbcppp::IMessage*> msg_index_;
};

}  // namespace jcan
