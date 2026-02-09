#pragma once

#include <imgui.h>

#include <algorithm>
#include <format>

#include "app_state.hpp"

namespace jcan::widgets {

inline std::string hex_data(const can_frame& f) {
  std::string out;
  uint8_t len = frame_payload_len(f);
  out.reserve(len * 3);
  for (uint8_t i = 0; i < len; ++i) {
    if (i) out += ' ';
    out += std::format("{:02X}", f.data[i]);
  }
  return out;
}

inline bool frame_matches_filter(const can_frame& f, const char* filter,
                                 const dbc_engine& dbc) {
  if (filter[0] == '\0') return true;
  std::string filt(filter);
  for (auto& c : filt) c = static_cast<char>(std::toupper(c));

  auto id_str =
      f.extended ? std::format("{:08X}", f.id) : std::format("{:03X}", f.id);
  if (id_str.find(filt) != std::string::npos) return true;

  if (dbc.loaded()) {
    auto name = dbc.message_name(f.id);
    if (!name.empty()) {
      for (auto& c : name) c = static_cast<char>(std::toupper(c));
      if (name.find(filt) != std::string::npos) return true;
    }
  }

  return false;
}

inline std::string format_relative_time(const can_frame& f,
                                        const app_state& state) {
  if (!state.has_first_frame) return "---";
  auto delta = f.timestamp - state.first_frame_time;
  auto total_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();
  if (total_ms < 0) total_ms = 0;
  int secs = static_cast<int>(total_ms / 1000);
  int ms = static_cast<int>(total_ms % 1000);
  if (secs < 60) return std::format("{}.{:03d}", secs, ms);
  int mins = secs / 60;
  secs %= 60;
  return std::format("{}:{:02d}.{:03d}", mins, secs, ms);
}

inline void monitor_row_context_menu(const can_frame& f, app_state& state,
                                     const char* popup_id) {
  if (ImGui::BeginPopup(popup_id)) {
    auto id_str =
        f.extended ? std::format("{:08X}", f.id) : std::format("{:03X}", f.id);
    auto data_str = hex_data(f);

    if (ImGui::MenuItem("Copy ID")) {
      ImGui::SetClipboardText(id_str.c_str());
    }
    if (ImGui::MenuItem("Copy Data")) {
      ImGui::SetClipboardText(data_str.c_str());
    }
    if (ImGui::MenuItem("Copy ID + Data")) {
      auto full = std::format("{} [{}] {}", id_str, f.dlc, data_str);
      ImGui::SetClipboardText(full.c_str());
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Filter to this ID")) {
      std::snprintf(state.filter_text, sizeof(state.filter_text), "%s",
                    id_str.c_str());
    }
    if (state.dbc.loaded()) {
      auto msg = state.dbc.message_name(f.id);
      if (!msg.empty()) {
        auto decoded = state.dbc.decode(f);
        if (ImGui::BeginMenu("Copy Signal")) {
          for (const auto& sig : decoded) {
            auto label =
                std::format("{}={:.2f}{}", sig.name, sig.value, sig.unit);
            if (ImGui::MenuItem(label.c_str())) {
              ImGui::SetClipboardText(label.c_str());
            }
          }
          ImGui::EndMenu();
        }
      }
    }
    ImGui::EndPopup();
  }
}

inline void draw_monitor_live(app_state& state) {
  ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Bus Monitor - Live")) {
    if (ImGui::Button(state.monitor_freeze ? "Resume" : "Freeze"))
      state.toggle_freeze();
    ImGui::SameLine();
    if (ImGui::Button("Clear")) state.clear_monitor();
    if (!state.frozen_rows.empty()) {
      ImGui::SameLine();
      if (ImGui::Button("Clear Delta")) state.frozen_rows.clear();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##filter", "Filter (ID or name)...",
                             state.filter_text, sizeof(state.filter_text));
    ImGui::SameLine();
    ImGui::Text("Rows: %zu", state.monitor_rows.size());

    ImGui::Separator();

    constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                           ImGuiTableFlags_Sortable |
                           ImGuiTableFlags_SizingStretchProp;

    const bool have_dbc = state.dbc.loaded();
    const int col_count = have_dbc ? 8 : 6;

    if (ImGui::BeginTable("##live_table", col_count, flags)) {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn(
          "ID",
          ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort,
          80);
      ImGui::TableSetupColumn(
          "Ext",
          ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 30);
      ImGui::TableSetupColumn(
          "DLC",
          ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 35);
      ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch |
                                          ImGuiTableColumnFlags_NoSort);
      if (have_dbc) {
        ImGui::TableSetupColumn(
            "Message",
            ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
            110);
        ImGui::TableSetupColumn("Signals", ImGuiTableColumnFlags_WidthStretch |
                                               ImGuiTableColumnFlags_NoSort);
      }
      ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 65);
      ImGui::TableSetupColumn("dt(ms)", ImGuiTableColumnFlags_WidthFixed, 70);
      ImGui::TableHeadersRow();

      struct sort_row {
        int idx;
        uint32_t id;
        uint64_t count;
        float dt_ms;
      };
      std::vector<sort_row> sorted;
      sorted.reserve(state.monitor_rows.size());
      for (int i = 0; i < static_cast<int>(state.monitor_rows.size()); ++i) {
        const auto& row = state.monitor_rows[i];
        if (!frame_matches_filter(row.frame, state.filter_text, state.dbc))
          continue;
        sorted.push_back({i, row.frame.id, row.count, row.dt_ms});
      }

      int count_col = have_dbc ? 6 : 4;
      int dt_col = have_dbc ? 7 : 5;
      if (auto* specs = ImGui::TableGetSortSpecs()) {
        int col = 0;
        auto dir = ImGuiSortDirection_Ascending;
        if (specs->SpecsCount > 0) {
          col = specs->Specs[0].ColumnIndex;
          dir = specs->Specs[0].SortDirection;
        }
        std::sort(sorted.begin(), sorted.end(),
                  [&](const sort_row& a, const sort_row& b) {
                    bool less;
                    if (col == 0)
                      less = a.id < b.id;
                    else if (col == count_col)
                      less = a.count < b.count;
                    else if (col == dt_col)
                      less = a.dt_ms < b.dt_ms;
                    else
                      less = a.id < b.id;
                    return dir == ImGuiSortDirection_Ascending ? less : !less;
                  });
        specs->SpecsDirty = false;
      }

      for (const auto& sr : sorted) {
        const auto& row = state.monitor_rows[sr.idx];

        const app_state::frame_row* frozen_row = nullptr;
        if (!state.frozen_rows.empty()) {
          for (const auto& fr : state.frozen_rows) {
            if (fr.frame.id == row.frame.id &&
                fr.frame.extended == row.frame.extended) {
              frozen_row = &fr;
              break;
            }
          }
        }

        auto id_str = row.frame.extended ? std::format("{:08X}", row.frame.id)
                                         : std::format("{:03X}", row.frame.id);

        ImGui::TableNextRow();
        if (!state.frozen_rows.empty() && !frozen_row) {
          ImGui::TableSetBgColor(
              ImGuiTableBgTarget_RowBg1,
              ImGui::GetColorU32(ImVec4(0.2f, 0.4f, 0.1f, 0.4f)));
        }
        ImGui::TableNextColumn();
        if (state.mono_font) ImGui::PushFont(state.mono_font);
        ImGui::TextUnformatted(id_str.c_str());
        if (state.mono_font) ImGui::PopFont();

        ImGui::TableNextColumn();
        ImGui::Text("%s", row.frame.extended ? "X" : "");
        ImGui::TableNextColumn();
        if (state.mono_font) ImGui::PushFont(state.mono_font);
        if (row.frame.fd)
          ImGui::Text("%u*", frame_payload_len(row.frame));
        else
          ImGui::Text("%u", row.frame.dlc);
        if (state.mono_font) ImGui::PopFont();

        ImGui::TableNextColumn();
        if (state.mono_font) ImGui::PushFont(state.mono_font);
        if (frozen_row) {
          uint8_t len = frame_payload_len(row.frame);
          for (uint8_t i = 0; i < len; ++i) {
            if (i) ImGui::SameLine(0, 3);
            uint8_t frozen_len = frame_payload_len(frozen_row->frame);
            bool changed = (i >= frozen_len ||
                            row.frame.data[i] != frozen_row->frame.data[i]);
            if (changed)
              ImGui::PushStyleColor(ImGuiCol_Text,
                                    ImVec4(1.0f, 0.8f, 0.0f, 1.0f));
            ImGui::Text("%02X", row.frame.data[i]);
            if (changed) ImGui::PopStyleColor();
          }
        } else {
          ImGui::TextUnformatted(hex_data(row.frame).c_str());
        }
        if (state.mono_font) ImGui::PopFont();

        if (have_dbc) {
          ImGui::TableNextColumn();
          auto msg = state.dbc.message_name(row.frame.id);
          if (!msg.empty()) ImGui::TextUnformatted(msg.c_str());

          ImGui::TableNextColumn();
          auto decoded = state.dbc.decode(row.frame);
          std::string sig_str;
          for (std::size_t si = 0; si < decoded.size(); ++si) {
            if (si) sig_str += "  ";
            sig_str += std::format("{}={:.2f}{}", decoded[si].name,
                                   decoded[si].value, decoded[si].unit);
          }
          ImGui::TextUnformatted(sig_str.c_str());
        }

        ImGui::TableNextColumn();
        ImGui::Text("%lu", row.count);
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", row.dt_ms);

        auto popup_id = std::format("##ctx_live_{}", sr.idx);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup))
          ImGui::SetItemTooltip("Right-click for options");
        ImGui::OpenPopupOnItemClick(popup_id.c_str(),
                                    ImGuiPopupFlags_MouseButtonRight);
        monitor_row_context_menu(row.frame, state, popup_id.c_str());
      }
      ImGui::EndTable();
    }
  }
  ImGui::End();
}

inline void draw_monitor_scrollback(app_state& state) {
  ImGui::SetNextWindowSize(ImVec2(700, 300), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Bus Monitor - Scrollback")) {
    ImGui::Text("Frames: %zu / %zu", state.scrollback.size(),
                state.k_max_scrollback);
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &state.monitor_autoscroll);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##sb_filter", "Filter (ID or name)...",
                             state.scrollback_filter_text,
                             sizeof(state.scrollback_filter_text));

    ImGui::Separator();

    constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                           ImGuiTableFlags_ScrollY |
                           ImGuiTableFlags_SizingStretchProp;

    if (ImGui::BeginTable("##scroll_table", 5, flags)) {
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableSetupColumn("DLC", ImGuiTableColumnFlags_WidthFixed, 35);
      ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 100);
      ImGui::TableHeadersRow();

      const bool has_sb_filter = state.scrollback_filter_text[0] != '\0';
      static std::vector<int> sb_filt_idx;
      if (has_sb_filter) {
        sb_filt_idx.clear();
        sb_filt_idx.reserve(state.scrollback.size());
        for (int i = 0; i < static_cast<int>(state.scrollback.size()); ++i) {
          if (frame_matches_filter(state.scrollback[i],
                                   state.scrollback_filter_text, state.dbc))
            sb_filt_idx.push_back(i);
        }
      }

      const int sb_total = has_sb_filter
                               ? static_cast<int>(sb_filt_idx.size())
                               : static_cast<int>(state.scrollback.size());
      ImGuiListClipper clipper;
      clipper.Begin(sb_total);
      while (clipper.Step()) {
        for (int di = clipper.DisplayStart; di < clipper.DisplayEnd; ++di) {
          int i = has_sb_filter ? sb_filt_idx[di] : di;
          const auto& f = state.scrollback[static_cast<std::size_t>(i)];

          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%d", i);
          ImGui::TableNextColumn();
          if (state.mono_font) ImGui::PushFont(state.mono_font);
          if (f.extended)
            ImGui::Text("%08X", f.id);
          else
            ImGui::Text("%03X", f.id);
          if (state.mono_font) ImGui::PopFont();
          ImGui::TableNextColumn();
          if (state.mono_font) ImGui::PushFont(state.mono_font);
          if (f.fd)
            ImGui::Text("%u*", frame_payload_len(f));
          else
            ImGui::Text("%u", f.dlc);
          if (state.mono_font) ImGui::PopFont();
          ImGui::TableNextColumn();
          if (state.mono_font) ImGui::PushFont(state.mono_font);
          ImGui::TextUnformatted(hex_data(f).c_str());
          if (state.mono_font) ImGui::PopFont();
          ImGui::TableNextColumn();
          auto ts_str = format_relative_time(f, state);
          if (state.mono_font) ImGui::PushFont(state.mono_font);
          ImGui::TextUnformatted(ts_str.c_str());
          if (state.mono_font) ImGui::PopFont();

          auto popup_id = std::format("##ctx_sb_{}", di);
          ImGui::OpenPopupOnItemClick(popup_id.c_str(),
                                      ImGuiPopupFlags_MouseButtonRight);
          monitor_row_context_menu(f, state, popup_id.c_str());
        }
      }

      if (state.monitor_autoscroll &&
          ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20)
        ImGui::SetScrollHereY(1.0f);

      ImGui::EndTable();
    }
  }
  ImGui::End();
}

}
