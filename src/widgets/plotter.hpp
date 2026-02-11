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

/// Persistent state for the plotter window.
struct plotter_state {
  std::vector<strip_chart_state> charts;
  channel_list_state channel_list;
  int active_chart{0};  // which chart new signals get added to

  plotter_state() {
    // Start with one empty chart
    charts.emplace_back();
  }
};

/// Returns true if a signal_key is plotted on any chart.
inline bool is_signal_on_any_chart(const plotter_state& ps,
                                   const signal_key& key) {
  for (const auto& chart : ps.charts) {
    for (const auto& tr : chart.traces) {
      if (tr.key == key) return true;
    }
  }
  return false;
}

/// Toggle a signal on the active chart.
inline void toggle_signal(plotter_state& ps, const signal_key& key) {
  if (ps.charts.empty()) ps.charts.emplace_back();
  int idx =
      std::clamp(ps.active_chart, 0, static_cast<int>(ps.charts.size()) - 1);
  auto& chart = ps.charts[static_cast<std::size_t>(idx)];

  // Check if already on this chart
  for (auto it = chart.traces.begin(); it != chart.traces.end(); ++it) {
    if (it->key == key) {
      chart.traces.erase(it);
      return;
    }
  }

  // Add with auto-color
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

  if (!state.any_dbc_loaded()) {
    ImGui::TextDisabled(
        "No DBC loaded -- load a DBC file to see signal channels");
    ImGui::End();
    return;
  }

  float sidebar_width = 220.0f * state.ui_scale;

  // --- Channel list sidebar ---
  if (ImGui::BeginChild("##sidebar", ImVec2(sidebar_width, 0), true)) {
    auto msg_name_fn = [&state](uint32_t id) -> std::string {
      return state.any_message_name(id);
    };

    auto is_on = [&ps](const signal_key& key) -> bool {
      return is_signal_on_any_chart(ps, key);
    };

    draw_channel_list(ps.channel_list, state.signals, msg_name_fn, is_on);
  }
  ImGui::EndChild();

  ImGui::SameLine();

  // --- Charts area (no scroll — charts fill the space exactly) ---
  if (ImGui::BeginChild(
          "##charts_area", ImVec2(0, 0), false,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    // Toolbar
    if (ImGui::SmallButton("+ Chart")) {
      ps.charts.emplace_back();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu chart%s, %zu channels, %zu samples)",
                        ps.charts.size(), ps.charts.size() != 1 ? "s" : "",
                        state.signals.channel_count(),
                        state.signals.total_samples());

    // Pause / Live toggle
    if (!ps.charts.empty()) {
      ImGui::SameLine();
      if (ps.charts[0].live_follow) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 0.7f));
        if (ImGui::SmallButton("LIVE")) {
          for (auto& c : ps.charts) c.live_follow = false;
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Click or press Space to pause");
      } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.3f, 0.1f, 0.7f));
        if (ImGui::SmallButton("PAUSED")) {
          for (auto& c : ps.charts) {
            c.live_follow = true;
            c.view_end_offset_sec = 0.0f;
          }
        }
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip("Click or press Space to resume live");
      }
    }

    ImGui::Separator();

    // Draw each chart — divide available space evenly
    int n_charts = static_cast<int>(ps.charts.size());
    if (n_charts == 0) {
      ImGui::TextDisabled("Click '+ Chart' or select signals from the sidebar");
    } else {
      float avail_h = ImGui::GetContentRegionAvail().y;
      // Each chart has: header line (~ImGui::GetFrameHeight) + canvas + legend
      // line (~ImGui::GetTextLineHeightWithSpacing) + spacing
      float header_h = ImGui::GetFrameHeight() + 2.0f;
      float legend_h = ImGui::GetTextLineHeightWithSpacing() + 2.0f;
      float per_chart_overhead = header_h + legend_h;
      float total_overhead = per_chart_overhead * static_cast<float>(n_charts);
      float chart_h = std::max(
          40.0f, (avail_h - total_overhead) / static_cast<float>(n_charts));

      for (int ci = 0; ci < n_charts; ++ci) {
        auto& chart = ps.charts[static_cast<std::size_t>(ci)];

        ImGui::PushID(ci);

        // Chart header — thin selectable bar instead of collapsing header
        bool is_active = (ci == ps.active_chart);
        {
          std::string label = std::format("  Chart {} ({} trace{})", ci + 1,
                                          chart.traces.size(),
                                          chart.traces.size() != 1 ? "s" : "");
          if (is_active) {
            ImGui::PushStyleColor(ImGuiCol_Header,
                                  ImVec4(0.2f, 0.4f, 0.6f, 0.5f));
          }
          if (ImGui::Selectable(label.c_str(), is_active,
                                ImGuiSelectableFlags_None,
                                ImVec2(0, ImGui::GetTextLineHeight()))) {
            ps.active_chart = ci;
          }
          if (is_active) ImGui::PopStyleColor();

          // Drop target on header — drop signal onto this chart
          if (ImGui::BeginDragDropTarget()) {
            if (const auto* payload =
                    ImGui::AcceptDragDropPayload("SIGNAL_KEY")) {
              auto* key = *static_cast<const signal_key* const*>(payload->Data);
              ps.active_chart = ci;
              // Add if not already on this chart
              bool already = false;
              for (const auto& tr : chart.traces) {
                if (tr.key == *key) {
                  already = true;
                  break;
                }
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

          // Right-click to remove
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

        draw_strip_chart(chart, state.signals, chart_h);

        ImGui::PopID();
      }
    }

    // Keyboard: Space to toggle live
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_Space)) {
      for (auto& c : ps.charts) {
        if (c.live_follow) {
          c.live_follow = false;
        } else {
          c.live_follow = true;
          c.view_end_offset_sec = 0.0f;
        }
      }
    }

    // W to zoom out full
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_W)) {
      for (auto& c : ps.charts) {
        c.view_duration_sec = static_cast<float>(state.signals.max_seconds());
        c.view_end_offset_sec = 0.0f;
        c.live_follow = true;
      }
    }
  }
  ImGui::EndChild();

  ImGui::End();
}

}  // namespace jcan::widgets
