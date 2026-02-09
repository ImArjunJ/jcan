#pragma once

#include <imgui.h>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

#include "app_state.hpp"

namespace jcan::widgets {

inline void draw_signal_watcher(app_state& state) {
  ImGui::SetNextWindowSize(ImVec2(500, 350), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Signal Watcher")) {
    ImGui::End();
    return;
  }

  auto& sw = state.signal_watcher;

  ImGui::Checkbox("Auto-track all signals", &sw.auto_add);
  ImGui::SameLine();
  if (ImGui::Button("Clear all")) sw.traces.clear();
  ImGui::SameLine();
  ImGui::Text("Tracking: %zu signals", sw.traces.size());
  ImGui::Separator();

  if (sw.auto_add && state.dbc.loaded()) {
    for (const auto& row : state.monitor_rows) {
      auto decoded = state.dbc.decode(row.frame);
      auto msg_name = state.dbc.message_name(row.frame.id);
      for (const auto& sig : decoded) {
        auto key = std::format("{}.{}", msg_name, sig.name);
        auto& trace = sw.traces[key];
        trace.name = sig.name;
        trace.unit = sig.unit;
        trace.push(static_cast<float>(sig.value));
      }
    }
  }

  for (auto& [key, trace] : sw.traces) {
    if (trace.samples.empty()) continue;

    static thread_local std::vector<float> plot_buf;
    plot_buf.assign(trace.samples.begin(), trace.samples.end());

    float vmin = plot_buf[0], vmax = plot_buf[0];
    double vsum = 0.0;
    for (float v : plot_buf) {
      if (v < vmin) vmin = v;
      if (v > vmax) vmax = v;
      vsum += static_cast<double>(v);
    }
    float vavg =
        static_cast<float>(vsum / static_cast<double>(plot_buf.size()));

    auto overlay = std::format("{:.2f} {}", trace.last_value, trace.unit);
    ImGui::PlotLines(key.c_str(), plot_buf.data(),
                     static_cast<int>(plot_buf.size()), 0, overlay.c_str(),
                     FLT_MAX, FLT_MAX, ImVec2(-1, 60));

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Min: %.3f  Max: %.3f  Avg: %.3f %s", vmin, vmax, vavg,
                        trace.unit.c_str());
    }
  }

  ImGui::End();
}

}
