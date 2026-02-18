#pragma once

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <format>
#include <string>
#include <vector>

#include "signal_store.hpp"
#include "theme.hpp"

namespace jcan::widgets {

struct chart_trace {
  signal_key key;
  ImU32 color{IM_COL32(100, 200, 255, 255)};
  bool visible{true};
};

struct strip_chart_state {
  std::vector<chart_trace> traces;
  bool live_follow{true};
  float view_duration_sec{10.0f};
  float view_end_offset_sec{0.0f};

  bool y_auto{true};
  double y_min{0.0};
  double y_max{1.0};

  bool dragging{false};
  float drag_start_offset{0.0f};
  ImVec2 drag_start_pos{};

  signal_sample::clock::time_point pause_time{};

  bool cursor_active{false};
  float cursor_time_sec{0.0f};

  static int next_id() {
    static int id = 0;
    return id++;
  }
  int id{next_id()};
};

inline ImU32 trace_color(int index) {
  static const ImU32 palette[] = {
      IM_COL32(100, 255, 100, 255), IM_COL32(100, 200, 255, 255),
      IM_COL32(255, 100, 100, 255), IM_COL32(255, 200, 50, 255),
      IM_COL32(200, 100, 255, 255), IM_COL32(255, 150, 50, 255),
      IM_COL32(50, 255, 200, 255),  IM_COL32(255, 100, 200, 255),
      IM_COL32(150, 150, 255, 255), IM_COL32(200, 255, 100, 255),
  };
  constexpr int n = sizeof(palette) / sizeof(palette[0]);
  return palette[index % n];
}

inline int global_trace_count(const std::vector<strip_chart_state>& charts) {
  int total = 0;
  for (const auto& c : charts) total += static_cast<int>(c.traces.size());
  return total;
}

inline bool draw_strip_chart(strip_chart_state& chart,
                             const signal_store& store,
                             const jcan::semantic_colors& colors,
                             float height = 200.0f) {
  auto real_now = signal_sample::clock::now();

  if (!chart.live_follow &&
      chart.pause_time == signal_sample::clock::time_point{}) {
    chart.pause_time = real_now;
  }
  if (chart.live_follow) {
    chart.pause_time = {};
  }

  auto now = chart.live_follow ? real_now : chart.pause_time;

  ImGui::PushID(chart.id);

  ImVec2 avail = ImGui::GetContentRegionAvail();
  float chart_width = avail.x;
  if (chart_width < 100.0f) chart_width = 100.0f;
  if (height < 60.0f) height = 60.0f;

  ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  ImVec2 canvas_size(chart_width, height);
  ImVec2 canvas_end(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y);

  auto* draw = ImGui::GetWindowDrawList();

  draw->AddRectFilled(canvas_pos, canvas_end, colors.chart_bg);
  draw->AddRect(canvas_pos, canvas_end, colors.chart_border);

  if (chart.traces.empty()) {
    const char* hint = "Drag a signal from the sidebar to add it here";
    auto hint_size = ImGui::CalcTextSize(hint);
    ImVec2 hint_pos(canvas_pos.x + (canvas_size.x - hint_size.x) * 0.5f,
                    canvas_pos.y + (canvas_size.y - hint_size.y) * 0.5f);
    draw->AddText(hint_pos, colors.chart_grid_text, hint);
  }

  ImGui::InvisibleButton("##chart_area", canvas_size);
  bool hovered = ImGui::IsItemHovered();
  bool active = ImGui::IsItemActive();

  bool drop_accepted = false;
  signal_key dropped_key;
  if (ImGui::BeginDragDropTarget()) {
    if (const auto* payload = ImGui::AcceptDragDropPayload("SIGNAL_KEY")) {
      auto* key = *static_cast<const signal_key* const*>(payload->Data);
      dropped_key = *key;
      drop_accepted = true;
    }
    ImGui::EndDragDropTarget();
  }

  float view_end_sec;
  if (chart.live_follow) {
    view_end_sec = 0.0f;
    chart.view_end_offset_sec = 0.0f;
  } else {
    view_end_sec = chart.view_end_offset_sec;
  }
  float view_start_sec = view_end_sec + chart.view_duration_sec;

  auto time_to_x = [&](float sec_ago) -> float {
    float frac = 1.0f - (sec_ago - view_end_sec) / chart.view_duration_sec;
    return canvas_pos.x + frac * canvas_size.x;
  };

  ImGuiIO& io = ImGui::GetIO();

  if (hovered) {
    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

    if (io.MouseWheel != 0.0f) {
      float zoom = std::pow(1.15f, -io.MouseWheel);

      float mouse_frac = (io.MousePos.x - canvas_pos.x) / canvas_size.x;
      float mouse_sec_ago =
          view_end_sec + (1.0f - mouse_frac) * chart.view_duration_sec;

      chart.view_duration_sec *= zoom;
      chart.view_duration_sec =
          std::clamp(chart.view_duration_sec, 0.1f, 36000.0f);

      chart.view_end_offset_sec =
          mouse_sec_ago - (1.0f - mouse_frac) * chart.view_duration_sec;
      if (chart.view_end_offset_sec < 0.0f) chart.view_end_offset_sec = 0.0f;
      chart.live_follow = (chart.view_end_offset_sec < 0.01f);
    }
  }

  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    chart.dragging = true;
    chart.drag_start_offset = chart.view_end_offset_sec;
    chart.drag_start_pos = io.MousePos;
    chart.live_follow = false;
  }
  if (chart.dragging) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      float dx = io.MousePos.x - chart.drag_start_pos.x;
      float sec_per_px = chart.view_duration_sec / canvas_size.x;
      chart.view_end_offset_sec = chart.drag_start_offset + dx * sec_per_px;
      if (chart.view_end_offset_sec < 0.0f) chart.view_end_offset_sec = 0.0f;
    } else {
      chart.dragging = false;
      chart.live_follow = (chart.view_end_offset_sec < 0.01f);
    }
  }

  if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
    chart.live_follow = true;
    chart.view_end_offset_sec = 0.0f;
    chart.dragging = false;
  }

  if (chart.live_follow) {
    view_end_sec = 0.0f;
    chart.view_end_offset_sec = 0.0f;
  } else {
    view_end_sec = chart.view_end_offset_sec;
  }
  view_start_sec = view_end_sec + chart.view_duration_sec;

  if (chart.y_auto && !chart.traces.empty()) {
    double y_lo = 1e30, y_hi = -1e30;
    bool has_data = false;

    for (const auto& tr : chart.traces) {
      if (!tr.visible) continue;
      const auto* samps = store.samples(tr.key);
      if (!samps || samps->empty()) continue;

      for (const auto& s : *samps) {
        float age = std::chrono::duration<float>(now - s.time).count();
        if (age < view_start_sec && age >= view_end_sec) {
          y_lo = std::min(y_lo, s.value);
          y_hi = std::max(y_hi, s.value);
          has_data = true;
        }
      }
    }

    if (has_data) {
      double range = y_hi - y_lo;
      if (range < 1e-9) range = 1.0;
      double margin = range * 0.08;

      double target_lo = y_lo - margin;
      double target_hi = y_hi + margin;
      chart.y_min += (target_lo - chart.y_min) * 0.15;
      chart.y_max += (target_hi - chart.y_max) * 0.15;
    }
  }

  auto value_to_y = [&](double val) -> float {
    if (std::abs(chart.y_max - chart.y_min) < 1e-15)
      return canvas_pos.y + canvas_size.y * 0.5f;
    double frac = (val - chart.y_min) / (chart.y_max - chart.y_min);
    return canvas_pos.y + static_cast<float>(1.0 - frac) * canvas_size.y;
  };

  {
    float grid_step_sec = chart.view_duration_sec / 5.0f;

    float mag = std::pow(10.0f, std::floor(std::log10(grid_step_sec)));
    float norm = grid_step_sec / mag;
    if (norm < 1.5f)
      grid_step_sec = mag;
    else if (norm < 3.5f)
      grid_step_sec = 2.0f * mag;
    else if (norm < 7.5f)
      grid_step_sec = 5.0f * mag;
    else
      grid_step_sec = 10.0f * mag;

    float t = std::ceil(view_end_sec / grid_step_sec) * grid_step_sec;
    while (t <= view_start_sec) {
      float x = time_to_x(t);
      if (x >= canvas_pos.x && x <= canvas_end.x) {
        draw->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_end.y),
                      colors.chart_grid);

        std::string label;
        if (t < 0.01f && t > -0.01f)
          label = "now";
        else
          label = std::format("-{:.1f}s", t);
        draw->AddText(ImVec2(x + 2, canvas_end.y - 14),
                      colors.chart_grid_text, label.c_str());
      }
      t += grid_step_sec;
    }

    double y_range = chart.y_max - chart.y_min;
    if (y_range > 1e-15) {
      double y_step = y_range / 4.0;
      double y_mag = std::pow(10.0, std::floor(std::log10(y_step)));
      double y_norm = y_step / y_mag;
      if (y_norm < 1.5)
        y_step = y_mag;
      else if (y_norm < 3.5)
        y_step = 2.0 * y_mag;
      else if (y_norm < 7.5)
        y_step = 5.0 * y_mag;
      else
        y_step = 10.0 * y_mag;

      double yv = std::ceil(chart.y_min / y_step) * y_step;
      while (yv <= chart.y_max) {
        float y = value_to_y(yv);
        if (y >= canvas_pos.y && y <= canvas_end.y) {
          draw->AddLine(ImVec2(canvas_pos.x, y), ImVec2(canvas_end.x, y),
                        colors.chart_grid);
          auto lbl = std::format("{:.4g}", yv);
          draw->AddText(ImVec2(canvas_pos.x + 2, y - 14),
                        colors.chart_grid_text, lbl.c_str());
        }
        yv += y_step;
      }
    }
  }

  draw->PushClipRect(canvas_pos, canvas_end, true);

  for (const auto& tr : chart.traces) {
    if (!tr.visible) continue;
    const auto* samps = store.samples(tr.key);
    if (!samps || samps->empty()) continue;

    struct bin {
      float y_min, y_max, y_first, y_last;
      bool used{false};
    };
    int pixel_width = static_cast<int>(canvas_size.x);
    if (pixel_width < 1) pixel_width = 1;
    std::vector<bin> bins(static_cast<std::size_t>(pixel_width));

    for (const auto& s : *samps) {
      float age = std::chrono::duration<float>(now - s.time).count();
      if (age > view_start_sec || age < view_end_sec) continue;

      float x = time_to_x(age);
      int px = static_cast<int>(x - canvas_pos.x);
      if (px < 0 || px >= pixel_width) continue;

      float y = value_to_y(s.value);
      auto& b = bins[static_cast<std::size_t>(px)];
      if (!b.used) {
        b.y_min = b.y_max = b.y_first = b.y_last = y;
        b.used = true;
      } else {
        b.y_min = std::min(b.y_min, y);
        b.y_max = std::max(b.y_max, y);
        b.y_last = y;
      }
    }

    float prev_x = 0.0f;
    float prev_y = 0.0f;
    bool has_prev = false;
    for (int px = 0; px < pixel_width; ++px) {
      auto& b = bins[static_cast<std::size_t>(px)];
      if (!b.used) continue;
      float x = canvas_pos.x + static_cast<float>(px) + 0.5f;

      if (has_prev) {
        draw->AddLine(ImVec2(prev_x, prev_y), ImVec2(x, b.y_first), tr.color,
                      1.5f);
      }

      if (b.y_min != b.y_max) {
        draw->AddLine(ImVec2(x, b.y_min), ImVec2(x, b.y_max), tr.color, 1.5f);
      }

      prev_x = x;
      prev_y = b.y_last;
      has_prev = true;
    }
  }

  draw->PopClipRect();

  if (hovered && !chart.dragging) {
    float mouse_x = io.MousePos.x;
    if (mouse_x >= canvas_pos.x && mouse_x <= canvas_end.x) {
      draw->AddLine(ImVec2(mouse_x, canvas_pos.y),
                    ImVec2(mouse_x, canvas_end.y),
                    colors.chart_cursor);

      float frac = (mouse_x - canvas_pos.x) / canvas_size.x;
      float cursor_age = view_end_sec + (1.0f - frac) * chart.view_duration_sec;

      if (!chart.traces.empty()) {
        ImGui::BeginTooltip();
        ImGui::Text("-%.2fs", cursor_age);
        ImGui::Separator();
        for (const auto& tr : chart.traces) {
          if (!tr.visible) continue;
          const auto* samps = store.samples(tr.key);
          if (!samps || samps->empty()) continue;

          double best_val = 0.0;
          float best_dist = 1e30f;
          for (const auto& s : *samps) {
            float age = std::chrono::duration<float>(now - s.time).count();
            float dist = std::abs(age - cursor_age);
            if (dist < best_dist) {
              best_dist = dist;
              best_val = s.value;
            }
          }
          if (best_dist < chart.view_duration_sec) {
            ImVec4 col = ImGui::ColorConvertU32ToFloat4(tr.color);
            ImGui::TextColored(col, "%s: %.4g", tr.key.name.c_str(), best_val);
          }
        }
        ImGui::EndTooltip();
      }
    }
  }

  {
    if (!chart.live_follow) {
      ImGui::SameLine();
      if (ImGui::SmallButton("Live")) {
        chart.live_follow = true;
        chart.view_end_offset_sec = 0.0f;
      }
    }

    for (std::size_t i = 0; i < chart.traces.size(); ++i) {
      auto& tr = chart.traces[i];
      ImVec4 col = ImGui::ColorConvertU32ToFloat4(tr.color);

      if (i > 0) ImGui::SameLine();

      ImGui::PushStyleColor(ImGuiCol_Text, col);
      const auto* ch = store.channel(tr.key);
      if (ch) {
        ImGui::Text("%s: %.4g%s", tr.key.name.c_str(), ch->last_value,
                    ch->unit.empty() ? "" : (" " + ch->unit).c_str());
      } else {
        ImGui::TextUnformatted(tr.key.name.c_str());
      }
      ImGui::PopStyleColor();

      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        chart.traces.erase(chart.traces.begin() + static_cast<long>(i));
        --i;
      }
    }
  }

  if (drop_accepted) {
    bool already = false;
    for (const auto& tr : chart.traces) {
      if (tr.key == dropped_key) {
        already = true;
        break;
      }
    }
    if (!already) {
      chart_trace new_tr;
      new_tr.key = dropped_key;
      static int s_global_color_idx = 0;
      new_tr.color = trace_color(s_global_color_idx++);
      chart.traces.push_back(std::move(new_tr));
    }
  }

  ImGui::PopID();
  return true;
}

}  // namespace jcan::widgets
