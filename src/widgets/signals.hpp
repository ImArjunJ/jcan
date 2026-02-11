#pragma once

#include <imgui.h>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

#include "app_state.hpp"

namespace jcan::widgets {

inline void draw_signals(app_state& state) {
  ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Signals")) {
    ImGui::End();
    return;
  }

  if (!state.any_dbc_loaded()) {
    ImGui::TextDisabled("No DBC loaded -- load a DBC in the Connection window");
    ImGui::End();
    return;
  }

  static char sig_filter[128]{};
  ImGui::SetNextItemWidth(250);
  ImGui::InputTextWithHint("##sig_filter", "Search (signal, message, ID)...",
                           sig_filter, sizeof(sig_filter));
  ImGui::SameLine();
  ImGui::Text("Signals: ");

  struct sig_row {
    std::string message;
    uint32_t id;
    bool extended;
    std::string signal;
    double value;
    double raw;
    std::string unit;
    double minimum;
    double maximum;
  };

  std::vector<sig_row> rows;
  rows.reserve(state.monitor_rows.size() * 4);

  for (const auto& mr : state.monitor_rows) {
    auto msg_name = state.message_name_for(mr.frame.id, mr.frame.source);
    if (msg_name.empty()) continue;

    auto decoded = state.any_decode(mr.frame);
    for (const auto& sig : decoded) {
      rows.push_back(sig_row{
          .message = msg_name,
          .id = mr.frame.id,
          .extended = mr.frame.extended,
          .signal = sig.name,
          .value = sig.value,
          .raw = sig.raw,
          .unit = sig.unit,
          .minimum = sig.minimum,
          .maximum = sig.maximum,
      });
    }
  }

  std::string filt_upper;
  if (sig_filter[0] != '\0') {
    filt_upper = sig_filter;
    for (auto& c : filt_upper) c = static_cast<char>(std::toupper(c));
  }

  std::vector<int> visible;
  visible.reserve(rows.size());
  for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
    if (filt_upper.empty()) {
      visible.push_back(i);
      continue;
    }
    auto upper = [](std::string s) {
      for (auto& c : s) c = static_cast<char>(std::toupper(c));
      return s;
    };
    if (upper(rows[i].signal).find(filt_upper) != std::string::npos ||
        upper(rows[i].message).find(filt_upper) != std::string::npos) {
      visible.push_back(i);
      continue;
    }
    auto id_str = rows[i].extended ? std::format("{:08X}", rows[i].id)
                                   : std::format("{:03X}", rows[i].id);
    if (id_str.find(filt_upper) != std::string::npos) {
      visible.push_back(i);
    }
  }

  ImGui::SameLine();
  ImGui::Text("%zu", visible.size());

  ImGui::Separator();

  constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                         ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                         ImGuiTableFlags_Sortable |
                         ImGuiTableFlags_SizingStretchProp;

  if (ImGui::BeginTable("##signals_table", 7, flags)) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn(
        "Message",
        ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_DefaultSort,
        120);
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Signal", ImGuiTableColumnFlags_WidthFixed, 140);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 100);
    ImGui::TableSetupColumn(
        "Unit", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
        60);
    ImGui::TableSetupColumn("Raw", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Range", ImGuiTableColumnFlags_WidthStretch |
                                         ImGuiTableColumnFlags_NoSort);
    ImGui::TableHeadersRow();

    if (auto* specs = ImGui::TableGetSortSpecs()) {
      if (specs->SpecsCount > 0) {
        int col = specs->Specs[0].ColumnIndex;
        auto dir = specs->Specs[0].SortDirection;
        std::sort(visible.begin(), visible.end(), [&](int a, int b) {
          bool less;
          switch (col) {
            case 0:
              less = rows[a].message < rows[b].message;
              break;
            case 1:
              less = rows[a].id < rows[b].id;
              break;
            case 2:
              less = rows[a].signal < rows[b].signal;
              break;
            case 3:
              less = rows[a].value < rows[b].value;
              break;
            case 5:
              less = rows[a].raw < rows[b].raw;
              break;
            default:
              less = rows[a].message < rows[b].message;
              break;
          }
          return dir == ImGuiSortDirection_Ascending ? less : !less;
        });
        specs->SpecsDirty = false;
      }
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible.size()));
    while (clipper.Step()) {
      for (int ri = clipper.DisplayStart; ri < clipper.DisplayEnd; ++ri) {
        const auto& r = rows[visible[ri]];

        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r.message.c_str());

        ImGui::TableNextColumn();
        if (state.mono_font) ImGui::PushFont(state.mono_font);
        auto id_str = r.extended ? std::format("{:08X}", r.id)
                                 : std::format("{:03X}", r.id);
        ImGui::TextUnformatted(id_str.c_str());
        if (state.mono_font) ImGui::PopFont();

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r.signal.c_str());

        ImGui::TableNextColumn();
        if (state.mono_font) ImGui::PushFont(state.mono_font);
        ImGui::Text("%.4g", r.value);
        if (state.mono_font) ImGui::PopFont();

        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r.unit.c_str());

        ImGui::TableNextColumn();
        if (state.mono_font) ImGui::PushFont(state.mono_font);
        ImGui::Text("%.4g", r.raw);
        if (state.mono_font) ImGui::PopFont();

        ImGui::TableNextColumn();
        if (r.minimum != r.maximum) {
          ImGui::TextDisabled("[%.4g .. %.4g]", r.minimum, r.maximum);
        }
      }
    }
    ImGui::EndTable();
  }

  ImGui::End();
}

}  // namespace jcan::widgets
