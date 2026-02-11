#pragma once

#include <imgui.h>
#include <nfd.h>

#include <format>

#include "app_state.hpp"
#include "discovery.hpp"

namespace jcan::widgets {

inline void draw_connection_modal(app_state& state) {
  if (state.show_connection_modal) {
    ImGui::OpenPopup("Connection##modal");
    state.show_connection_modal = false;
  }

  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSizeConstraints(ImVec2(500, 0), ImVec2(500, FLT_MAX));

  bool modal_open = true;
  if (ImGui::BeginPopupModal("Connection##modal", &modal_open)) {
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

        ImGui::Indent(28.0f);
        if (slot->slot_dbc.loaded()) {
          auto fnames = slot->slot_dbc.filenames();
          for (const auto& fn : fnames) {
            ImGui::TextDisabled("DBC: %s", fn.c_str());
          }
          ImGui::SameLine();
          if (ImGui::SmallButton("Unload DBC")) {
            slot->slot_dbc.unload();
          }
        } else {
          ImGui::TextDisabled("DBC: (global)");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Load DBC...")) {
          nfdchar_t* out_path = nullptr;
          nfdfilteritem_t filters[] = {{"DBC Files", "dbc"}};
          if (NFD_OpenDialog(&out_path, filters, 1, nullptr) == NFD_OKAY) {
            slot->slot_dbc.load(out_path);
            NFD_FreePath(out_path);
          }
        }
        ImGui::Unindent(28.0f);

        ImGui::PopID();
      }
      if (remove_idx >= 0) state.disconnect_slot(remove_idx);
      ImGui::Separator();
    }

    if (ImGui::Button("Scan")) {
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
      ImGui::TextWrapped("No adapters found. Click 'Scan'.");
    }

    ImGui::Spacing();

    static const char* bitrate_labels[] = {
        "S0 - 10 kbit/s",  "S1 - 20 kbit/s",  "S2 - 50 kbit/s",
        "S3 - 100 kbit/s", "S4 - 125 kbit/s", "S5 - 250 kbit/s",
        "S6 - 500 kbit/s", "S7 - 800 kbit/s", "S8 - 1 Mbit/s",
    };
    ImGui::SetNextItemWidth(250);
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

    // Log directory
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Log Directory:");
    {
      static char log_dir_buf[512]{};
      static bool log_dir_init = false;
      if (!log_dir_init) {
        std::snprintf(log_dir_buf, sizeof(log_dir_buf), "%s",
                      state.log_dir.string().c_str());
        log_dir_init = true;
      }
      ImGui::SetNextItemWidth(-80);
      if (ImGui::InputText("##log_dir", log_dir_buf, sizeof(log_dir_buf),
                           ImGuiInputTextFlags_EnterReturnsTrue)) {
        state.log_dir = log_dir_buf;
      }
      ImGui::SameLine();
      if (ImGui::SmallButton("Browse")) {
        nfdchar_t* out_path = nullptr;
        if (NFD_PickFolder(&out_path, state.log_dir.string().c_str()) ==
            NFD_OKAY) {
          state.log_dir = out_path;
          std::snprintf(log_dir_buf, sizeof(log_dir_buf), "%s", out_path);
          NFD_FreePath(out_path);
        }
      }
    }
    if (state.logger.recording()) {
      ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Recording:");
      ImGui::TextWrapped("%s", state.session_log_path.c_str());
      ImGui::Text("Frames logged: %zu", state.logger.frame_count());
    }

    if (!state.status_text.empty()) {
      ImGui::Spacing();
      ImGui::TextWrapped("%s", state.status_text.c_str());
    }

    ImGui::EndPopup();
  }
}

}  // namespace jcan::widgets
