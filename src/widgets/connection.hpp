#pragma once

#include <imgui.h>

#include <format>

#include "app_state.hpp"
#include "discovery.hpp"

namespace jcan::widgets {

inline void draw_connection_modal(app_state& state) {
  if (!state.connected) {
    if (!state.show_connection_modal) {
      state.show_connection_modal = true;
    }
  }

  if (!state.show_connection_modal) return;

  ImGui::SetNextWindowSize(ImVec2(480, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Connection", &state.show_connection_modal)) {
    if (!state.adapter_slots.empty()) {
      ImGui::Text("Connected adapters:");
      int remove_idx = -1;
      for (int i = 0; i < static_cast<int>(state.adapter_slots.size()); ++i) {
        auto& slot = state.adapter_slots[static_cast<std::size_t>(i)];
        ImGui::PushID(i);

        bool is_tx = (i == state.tx_slot_idx);
        if (is_tx)
          ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "[TX]");
        else
          ImGui::TextDisabled("    ");
        ImGui::SameLine();
        ImGui::Text("%s", slot->desc.friendly_name.c_str());
        ImGui::SameLine();
        if (!is_tx && ImGui::SmallButton("Set TX")) {
          state.tx_sched.stop();
          state.tx_slot_idx = i;
          state.tx_sched.start(slot->hw);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Disconnect")) remove_idx = i;

        ImGui::PopID();
      }
      if (remove_idx >= 0) state.disconnect_slot(remove_idx);
      ImGui::Separator();
    }

    if (ImGui::Button("Scan for adapters")) {
      state.devices = discover_adapters();
      if (!state.devices.empty()) state.selected_device = 0;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu found)", state.devices.size());

    ImGui::Separator();

    if (!state.devices.empty()) {
      if (ImGui::BeginListBox("##adapters", ImVec2(-FLT_MIN, 120))) {
        for (int i = 0; i < static_cast<int>(state.devices.size()); ++i) {
          const auto& d = state.devices[static_cast<std::size_t>(i)];
          bool already = false;
          for (const auto& slot : state.adapter_slots) {
            if (slot->desc.port == d.port) {
              already = true;
              break;
            }
          }
          auto label = std::format("[{}] {} - {}{}", i, d.port, d.friendly_name,
                                   already ? " (connected)" : "");
          if (ImGui::Selectable(label.c_str(), state.selected_device == i))
            state.selected_device = i;
        }
        ImGui::EndListBox();
      }
    } else {
      ImGui::TextWrapped("No adapters found. Click 'Scan for adapters'.");
    }

    ImGui::Spacing();

    static const char* bitrate_labels[] = {
        "S0 - 10 kbit/s",  "S1 - 20 kbit/s",  "S2 - 50 kbit/s",
        "S3 - 100 kbit/s", "S4 - 125 kbit/s", "S5 - 250 kbit/s",
        "S6 - 500 kbit/s", "S7 - 800 kbit/s", "S8 - 1 Mbit/s",
    };
    ImGui::Combo("Bitrate", &state.selected_bitrate, bitrate_labels,
                 IM_ARRAYSIZE(bitrate_labels));

    ImGui::Spacing();

    bool can_connect = !state.devices.empty();
    if (!can_connect) ImGui::BeginDisabled();
    if (ImGui::Button("Connect", ImVec2(120, 0))) {
      state.connect();
    }
    if (!can_connect) ImGui::EndDisabled();
    if (state.adapter_slots.size() > 1) {
      ImGui::SameLine();
      if (ImGui::Button("Disconnect All", ImVec2(140, 0))) {
        state.disconnect();
      }
    }
    ImGui::SameLine();
    ImGui::TextWrapped("%s", state.status_text.c_str());
  }
  ImGui::End();
}

}
