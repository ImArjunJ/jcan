#pragma once

#include <imgui.h>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

#include "app_state.hpp"

namespace jcan::widgets {

inline void draw_statistics(app_state& state) {
  ImGui::SetNextWindowSize(ImVec2(450, 350), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Bus Statistics")) {
    ImGui::End();
    return;
  }

  auto& st = state.stats;

  static constexpr float bitrates[] = {
      10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000,
  };
  float bps = bitrates[std::clamp(state.selected_bitrate, 0, 8)];
  st.update(bps);

  auto total_text = std::format("Total frames: {}", st.total_frames);
  ImGui::TextUnformatted(total_text.c_str());
  ImGui::SameLine();
  if (ImGui::SmallButton("Reset")) st.reset();

  auto rate_text = std::format("Overall rate: {:.1f} msg/s", st.total_rate_hz);
  ImGui::TextUnformatted(rate_text.c_str());

  float load = std::clamp(st.bus_load_pct, 0.f, 100.f);
  auto load_label = std::format("{:.1f}%%", load);
  ImGui::Text("Bus load:");
  ImGui::SameLine();
  ImVec4 load_color;
  if (load < 50.f)
    load_color = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
  else if (load < 80.f)
    load_color = ImVec4(0.9f, 0.8f, 0.1f, 1.0f);
  else
    load_color = ImVec4(1.0f, 0.3f, 0.2f, 1.0f);
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, load_color);
  ImGui::ProgressBar(load / 100.f, ImVec2(200, 0), load_label.c_str());
  ImGui::PopStyleColor();

  if (st.error_frames > 0 || st.bus_off_count > 0 ||
      st.error_passive_count > 0) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    auto err_text =
        std::format("Errors: {}  Bus-off: {}  Error-passive: {}",
                    st.error_frames, st.bus_off_count, st.error_passive_count);
    ImGui::TextUnformatted(err_text.c_str());
    ImGui::PopStyleColor();
  }

  ImGui::Separator();

  constexpr auto flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                         ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
                         ImGuiTableFlags_Sortable |
                         ImGuiTableFlags_SizingStretchProp;

  const bool have_dbc = state.any_dbc_loaded();
  const int col_count = have_dbc ? 4 : 3;

  if (ImGui::BeginTable("##stats_table", col_count, flags)) {
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 80);
    if (have_dbc)
      ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableSetupColumn("Rate (Hz)", ImGuiTableColumnFlags_WidthFixed, 80);
    ImGui::TableHeadersRow();

    struct stats_row {
      uint32_t id;
      uint64_t count;
      float rate;
      uint8_t source;
    };
    std::vector<stats_row> rows;
    rows.reserve(st.per_id.size());
    for (const auto& [id, is] : st.per_id) {
      rows.push_back({id, is.total_count, is.rate_hz, is.last_source});
    }

    int count_col = have_dbc ? 2 : 1;
    int rate_col = have_dbc ? 3 : 2;
    if (auto* specs = ImGui::TableGetSortSpecs()) {
      int col = rate_col;
      auto dir = ImGuiSortDirection_Descending;
      if (specs->SpecsCount > 0) {
        col = specs->Specs[0].ColumnIndex;
        dir = specs->Specs[0].SortDirection;
      }
      std::sort(rows.begin(), rows.end(),
                [&](const stats_row& a, const stats_row& b) {
                  bool less;
                  if (col == 0)
                    less = a.id < b.id;
                  else if (col == count_col)
                    less = a.count < b.count;
                  else
                    less = a.rate < b.rate;
                  return dir == ImGuiSortDirection_Ascending ? less : !less;
                });
      specs->SpecsDirty = false;
    }

    for (const auto& r : rows) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (state.mono_font) ImGui::PushFont(state.mono_font);
      ImGui::Text("%03X", r.id);
      if (state.mono_font) ImGui::PopFont();
      if (have_dbc) {
        ImGui::TableNextColumn();
        auto name = state.message_name_for(r.id, r.source);
        if (!name.empty()) ImGui::TextUnformatted(name.c_str());
      }
      ImGui::TableNextColumn();
      auto cnt_text = std::format("{}", r.count);
      ImGui::TextUnformatted(cnt_text.c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%.1f", r.rate);
    }
    ImGui::EndTable();
  }

  ImGui::End();
}

}  // namespace jcan::widgets
