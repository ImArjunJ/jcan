#pragma once

#include <imgui.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "app_state.hpp"
#include "widgets/channel_list.hpp"
#include "widgets/strip_chart.hpp"

namespace jcan::widgets {

struct plotter_state {
  std::vector<strip_chart_state> charts;
  channel_list_state channel_list;
  int active_chart{0};

  bool shared_live{true};
  float shared_duration{10.0f};
  float shared_offset{0.0f};
  signal_sample::clock::time_point shared_pause_time{};

  plotter_state() { charts.emplace_back(); }

  void sync_to_chart(strip_chart_state& c) const {
    c.live_follow = shared_live;
    c.view_duration_sec = shared_duration;
    c.view_end_offset_sec = shared_offset;
    c.pause_time = shared_pause_time;
  }

  void sync_from_chart(const strip_chart_state& c) {
    shared_live = c.live_follow;
    shared_duration = c.view_duration_sec;
    shared_offset = c.view_end_offset_sec;
    shared_pause_time = c.pause_time;
  }
};

inline bool is_signal_on_any_chart(const plotter_state& ps,
                                   const signal_key& key) {
  for (const auto& chart : ps.charts)
    for (const auto& tr : chart.traces)
      if (tr.key == key) return true;
  return false;
}

inline void toggle_signal(plotter_state& ps, const signal_key& key) {
  if (ps.charts.empty()) ps.charts.emplace_back();
  int idx =
      std::clamp(ps.active_chart, 0, static_cast<int>(ps.charts.size()) - 1);
  auto& chart = ps.charts[static_cast<std::size_t>(idx)];

  for (auto it = chart.traces.begin(); it != chart.traces.end(); ++it) {
    if (it->key == key) {
      chart.traces.erase(it);
      return;
    }
  }

  chart_trace tr;
  tr.key = key;
  tr.color = trace_color(static_cast<int>(chart.traces.size()));
  chart.traces.push_back(std::move(tr));
}

inline void draw_plotter(app_state& state, plotter_state& ps) {
  ImGui::SetNextWindowSize(ImVec2(900, 500), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Analysis")) {
    ImGui::End();
    return;
  }

  if (!state.any_dbc_loaded() && state.signals.channel_count() == 0) {
    ImGui::TextDisabled(
        "No DBC loaded -- use File > Load DBC, Connection window, or drag & "
        "drop");
    ImGui::End();
    return;
  }

  float sidebar_width = 220.0f * state.ui_scale;

  if (ImGui::BeginChild("##sidebar", ImVec2(sidebar_width, 0), true)) {
    auto msg_name_fn = [&state](uint32_t id) -> std::string {
      auto name = state.any_message_name(id);
      if (name.empty() && id == 0) return "MoTec";
      return name;
    };
    auto is_on = [&ps](const signal_key& key) -> bool {
      return is_signal_on_any_chart(ps, key);
    };
    draw_channel_list(ps.channel_list, state.signals, msg_name_fn, is_on,
                      state.colors.channel_on_chart);
  }
  ImGui::EndChild();

  ImGui::SameLine();

  if (ImGui::BeginChild(
          "##charts_area", ImVec2(0, 0), false,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    if (ImGui::SmallButton("+ Chart")) {
      ps.charts.emplace_back();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu chart%s, %zu channels, %zu samples)",
                        ps.charts.size(), ps.charts.size() != 1 ? "s" : "",
                        state.signals.channel_count(),
                        state.signals.total_samples());

    if (!ps.charts.empty() && !state.log_mode) {
      ImGui::SameLine();
      if (ps.shared_live) {
        ImGui::PushStyleColor(ImGuiCol_Button, state.colors.live_button);
        if (ImGui::SmallButton("LIVE")) ps.shared_live = false;
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Click or press Space to pause");
      } else {
        ImGui::PushStyleColor(ImGuiCol_Button, state.colors.paused_button);
        if (ImGui::SmallButton("PAUSED")) {
          ps.shared_live = true;
          ps.shared_offset = 0.0f;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Click or press Space to resume live");
      }
    }
    if (state.log_mode) ps.shared_live = false;

    ImGui::Separator();

    int n_charts = static_cast<int>(ps.charts.size());
    if (n_charts == 0) {
      ImGui::TextDisabled("Click '+ Chart' or select signals from the sidebar");
    } else {
      float avail_h = ImGui::GetContentRegionAvail().y;
      float header_h = ImGui::GetFrameHeight() + 2.0f;
      float legend_h = ImGui::GetTextLineHeightWithSpacing() + 2.0f;
      float per_chart_overhead = header_h + legend_h;
      float total_overhead = per_chart_overhead * static_cast<float>(n_charts);
      float chart_h = std::max(
          40.0f, (avail_h - total_overhead) / static_cast<float>(n_charts));

      for (int ci = 0; ci < n_charts; ++ci) {
        auto& chart = ps.charts[static_cast<std::size_t>(ci)];
        ImGui::PushID(ci);
        ps.sync_to_chart(chart);

        bool is_active = (ci == ps.active_chart);
        {
          std::string label = std::format("  Chart {} ({} trace{})", ci + 1,
                                          chart.traces.size(),
                                          chart.traces.size() != 1 ? "s" : "");
          if (is_active)
            ImGui::PushStyleColor(ImGuiCol_Header,
                                  state.colors.active_chart_header);
          if (ImGui::Selectable(label.c_str(), is_active,
                                ImGuiSelectableFlags_None,
                                ImVec2(0, ImGui::GetTextLineHeight())))
            ps.active_chart = ci;
          if (is_active) ImGui::PopStyleColor();

          if (ImGui::BeginDragDropTarget()) {
            if (const auto* payload =
                    ImGui::AcceptDragDropPayload("SIGNAL_KEY")) {
              auto* key = *static_cast<const signal_key* const*>(payload->Data);
              ps.active_chart = ci;
              bool already = false;
              for (const auto& tr : chart.traces)
                if (tr.key == *key) {
                  already = true;
                  break;
                }
              if (!already) {
                chart_trace tr;
                tr.key = *key;
                tr.color = trace_color(static_cast<int>(chart.traces.size()));
                chart.traces.push_back(std::move(tr));
              }
            }
            ImGui::EndDragDropTarget();
          }

          if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && n_charts > 1) {
            ps.charts.erase(ps.charts.begin() + ci);
            if (ps.active_chart >= static_cast<int>(ps.charts.size()))
              ps.active_chart =
                  std::max(0, static_cast<int>(ps.charts.size()) - 1);
            --ci;
            --n_charts;
            ImGui::PopID();
            continue;
          }
        }

        draw_strip_chart(chart, state.signals, state.colors, chart_h);
        ps.sync_from_chart(chart);
        ImGui::PopID();
      }
    }

    if (!state.log_mode &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_Space)) {
      if (ps.shared_live)
        ps.shared_live = false;
      else {
        ps.shared_live = true;
        ps.shared_offset = 0.0f;
      }
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_W)) {
      ps.shared_duration = static_cast<float>(state.signals.max_seconds());
      ps.shared_offset = 0.0f;
      if (!state.log_mode) ps.shared_live = true;
    }
  }
  ImGui::EndChild();

  ImGui::End();
}

}  // namespace jcan::widgets
