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
  bool pending_fit{false};

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
                                   const signal_key& key, int layer_idx = -1) {
  for (const auto& chart : ps.charts)
    for (const auto& tr : chart.traces)
      if (tr.key == key && tr.layer_idx == layer_idx) return true;
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
  tr.color = trace_color(global_trace_count(ps.charts));
  chart.traces.push_back(std::move(tr));
}

inline void draw_plotter(app_state& state, plotter_state& ps) {
  ImGui::SetNextWindowSize(ImVec2(900, 500), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Analysis")) {
    ImGui::End();
    return;
  }

  if (!state.any_dbc_loaded() && state.signals.channel_count() == 0) {
    ImGui::TextDisabled("Load a DBC file to decode and plot signals");
    ImGui::TextDisabled("File > Load DBC  |  Ctrl+O  |  Drag & drop .dbc file");
    ImGui::TextDisabled("Or import a MoTec .ld log via File > Import Log");
    ImGui::End();
    return;
  }

  float sidebar_width = 220.0f * state.ui_scale;

  std::vector<layer_store_ref> layer_refs;
  std::vector<overlay_store_info> visible_overlay_stores;
  for (int i = 0; i < static_cast<int>(state.overlay_layers.size()); ++i) {
    auto& layer = state.overlay_layers[i];
    layer_refs.push_back({&layer.signals, &layer.time_offset_sec, layer.visible, i});
    if (layer.visible)
      visible_overlay_stores.push_back({i, &layer.signals, layer.name});
  }

  if (ImGui::BeginChild("##sidebar", ImVec2(sidebar_width, 0), true)) {
    auto msg_name_fn = [&state](uint32_t id) -> std::string {
      auto name = state.any_message_name(id);
      if (name.empty() && id == 0) return "MoTec";
      return name;
    };
    auto is_on = [&ps](const signal_key& key, int layer_idx) -> bool {
      return is_signal_on_any_chart(ps, key, layer_idx);
    };
    draw_channel_list(ps.channel_list, state.signals, visible_overlay_stores,
                      msg_name_fn, is_on, state.colors.channel_on_chart);
  }
  ImGui::EndChild();

  ImGui::SameLine();

  if (ImGui::BeginChild(
          "##charts_area", ImVec2(0, 0), false,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    if (ImGui::SmallButton("+ Chart")) {
      ps.charts.emplace_back();
    }
    if (!state.overlay_layers.empty()) {
      ImGui::SameLine();
      if (ImGui::SmallButton("Reset Offsets")) {
        for (auto& ch : ps.charts)
          for (auto& tr : ch.traces)
            tr.time_offset_sec = 0.f;
      }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(%zu chart%s, %zu channels, %zu samples)  Scroll=Zoom  Drag=Pan  W=Fit%s%s",
                        ps.charts.size(), ps.charts.size() != 1 ? "s" : "",
                        state.signals.channel_count(),
                        state.signals.total_samples(),
                        state.overlay_layers.empty() ? "" : "  RClick+Drag=Shift Trace",
                        state.log_mode ? "" : "  Space=Pause");

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
                    ImGui::AcceptDragDropPayload("SIGNAL_DRAG")) {
              auto* drag = *static_cast<const signal_drag_payload* const*>(payload->Data);
              ps.active_chart = ci;
              bool already = false;
              for (const auto& tr : chart.traces)
                if (tr.key == drag->key && tr.layer_idx == drag->layer_idx) {
                  already = true;
                  break;
                }
              if (!already) {
                chart_trace tr;
                tr.key = drag->key;
                tr.layer_idx = drag->layer_idx;
                tr.color = trace_color(global_trace_count(ps.charts));
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

        draw_strip_chart(chart, state.signals, layer_refs, state.colors, chart_h);
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

    auto do_fit = [&]() {
      signal_sample::clock::time_point earliest{};
      signal_sample::clock::time_point latest{};
      bool found = false;
      auto scan = [&](const signal_store& store) {
        auto channels = store.all_channels();
        for (const auto* ch : channels) {
          const auto* samps = store.samples(ch->key);
          if (samps && !samps->empty()) {
            if (!found || samps->front().time < earliest)
              earliest = samps->front().time;
            if (!found || samps->back().time > latest)
              latest = samps->back().time;
            found = true;
          }
        }
      };
      scan(state.signals);
      for (const auto& ov : state.overlay_layers)
        scan(ov.signals);
      if (!found) return;

      float span = std::chrono::duration<float>(latest - earliest).count();
      if (span < 0.1f) span = 10.f;

      for (auto& c : ps.charts) {
        c.pause_time = latest;
        c.live_follow = false;
        c.view_duration_sec = span * 1.05f;
        c.view_end_offset_sec = 0.0f;
      }
      ps.shared_live = false;
      ps.shared_duration = span * 1.05f;
      ps.shared_offset = 0.0f;
      ps.shared_pause_time = latest;
    };

    if (ps.pending_fit) {
      do_fit();
      ps.pending_fit = false;
    }

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
        ImGui::IsKeyPressed(ImGuiKey_W)) {
      do_fit();
    }
  }
  ImGui::EndChild();

  ImGui::End();
}

}  // namespace jcan::widgets
