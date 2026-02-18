#pragma once

#include <imgui.h>
#include <nfd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "app_state.hpp"

namespace jcan::widgets {

struct source_editor_state {
  bool open{false};
  uint32_t job_id{0};
  std::string signal_name;
  int pending_tab{-1};
  int drag_idx{-1};
  bool dragging{false};
  double frozen_t_max{10.0};
  double frozen_v_min{0.0};
  double frozen_v_max{1.0};
};

inline void draw_table_chart(signal_source& src, float width, float height,
                             source_editor_state& ed,
                             const jcan::semantic_colors& colors) {
  auto& pts = src.table.points;

  double t_min_d = 0.0, t_max_d = 10.0;
  double v_min_d = 0.0, v_max_d = 1.0;
  if (ed.dragging) {
    t_max_d = ed.frozen_t_max;
    v_min_d = ed.frozen_v_min;
    v_max_d = ed.frozen_v_max;
  } else {
    if (!pts.empty()) {
      t_max_d = std::max(pts.back().time_sec * 1.15, 1.0);
      v_min_d = pts[0].value;
      v_max_d = pts[0].value;
      for (const auto& p : pts) {
        v_min_d = std::min(v_min_d, p.value);
        v_max_d = std::max(v_max_d, p.value);
      }
      double margin = (v_max_d - v_min_d) * 0.12;
      if (margin < 0.5)
        margin = std::max((std::abs(v_max_d) + std::abs(v_min_d)) * 0.05, 0.5);
      v_min_d -= margin;
      v_max_d += margin;
    }
  }

  float left_margin = 50.f;
  float bottom_margin = 20.f;
  float top_pad = 4.f;
  float right_pad = 8.f;

  ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
  ImVec2 canvas_sz(width, height);
  ImGui::InvisibleButton(
      "##chart", canvas_sz,
      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  bool hovered = ImGui::IsItemHovered();

  auto* dl = ImGui::GetWindowDrawList();
  ImU32 col_bg = colors.editor_bg;
  ImU32 col_grid = colors.editor_grid;
  ImU32 col_axis = colors.editor_axis;
  ImU32 col_line = colors.editor_line;
  ImU32 col_point = colors.editor_point;
  ImU32 col_point_hl = colors.editor_point_hl;
  ImU32 col_text = colors.editor_text;

  float px0 = canvas_pos.x + left_margin;
  float py0 = canvas_pos.y + top_pad;
  float px1 = canvas_pos.x + canvas_sz.x - right_pad;
  float py1 = canvas_pos.y + canvas_sz.y - bottom_margin;
  float pw = px1 - px0;
  float ph = py1 - py0;

  dl->AddRectFilled(canvas_pos,
                    {canvas_pos.x + canvas_sz.x, canvas_pos.y + canvas_sz.y},
                    col_bg);

  auto to_screen = [&](double t, double v) -> ImVec2 {
    float x =
        px0 + static_cast<float>((t - t_min_d) / (t_max_d - t_min_d) * pw);
    float y =
        py1 - static_cast<float>((v - v_min_d) / (v_max_d - v_min_d) * ph);
    return {x, y};
  };
  auto from_screen = [&](ImVec2 sp) -> std::pair<double, double> {
    double t = t_min_d + (sp.x - px0) / pw * (t_max_d - t_min_d);
    double v = v_min_d + (py1 - sp.y) / ph * (v_max_d - v_min_d);
    return {std::max(t, 0.0), v};
  };

  auto nice_step = [](double range, int max_ticks) -> double {
    if (range <= 0.0) return 1.0;
    double rough = range / std::max(max_ticks, 1);
    double mag = std::pow(10.0, std::floor(std::log10(rough)));
    double norm = rough / mag;
    double nice;
    if (norm <= 1.0) nice = 1.0;
    else if (norm <= 2.0) nice = 2.0;
    else if (norm <= 5.0) nice = 5.0;
    else nice = 10.0;
    return nice * mag;
  };

  auto fmt_val = [](double v, double step) -> std::string {
    if (step >= 1.0 && std::abs(v - std::round(v)) < 1e-9)
      return std::format("{:.0f}", v);
    if (step >= 0.1)
      return std::format("{:.1f}", v);
    if (step >= 0.01)
      return std::format("{:.2f}", v);
    return std::format("{:.3f}", v);
  };

  int max_ticks_x = std::clamp(static_cast<int>(pw / 80.f), 3, 10);
  int max_ticks_y = std::clamp(static_cast<int>(ph / 40.f), 3, 8);

  double t_step = nice_step(t_max_d - t_min_d, max_ticks_x);
  double t_start = std::ceil(t_min_d / t_step) * t_step;
  for (double tv = t_start; tv <= t_max_d; tv += t_step) {
    float x = px0 + static_cast<float>((tv - t_min_d) / (t_max_d - t_min_d) * pw);
    if (x < px0 || x > px1) continue;
    dl->AddLine({x, py0}, {x, py1}, col_grid);
    auto txt = fmt_val(tv, t_step) + "s";
    dl->AddText({x - 12.f, py1 + 3.f}, col_text, txt.c_str());
  }

  double v_step = nice_step(v_max_d - v_min_d, max_ticks_y);
  double v_start = std::ceil(v_min_d / v_step) * v_step;
  for (double vv = v_start; vv <= v_max_d; vv += v_step) {
    float y = py1 - static_cast<float>((vv - v_min_d) / (v_max_d - v_min_d) * ph);
    if (y < py0 || y > py1) continue;
    dl->AddLine({px0, y}, {px1, y}, col_grid);
    auto txt = fmt_val(vv, v_step);
    auto tsz = ImGui::CalcTextSize(txt.c_str());
    dl->AddText({px0 - tsz.x - 4.f, y - tsz.y * 0.5f}, col_text, txt.c_str());
  }

  dl->AddLine({px0, py0}, {px0, py1}, col_axis, 1.5f);
  dl->AddLine({px0, py1}, {px1, py1}, col_axis, 1.5f);

  if (pts.size() >= 2) {
    int steps = std::max(static_cast<int>(pw), 100);
    ImVec2 prev = to_screen(pts.front().time_sec, pts.front().value);
    for (int i = 1; i <= steps; ++i) {
      double t = t_min_d + (t_max_d - t_min_d) * static_cast<double>(i) /
                               static_cast<double>(steps);
      double ev_t = t;
      if (src.repeat && !pts.empty() && pts.back().time_sec > 0)
        ev_t = std::fmod(t, pts.back().time_sec);
      double v = src.table.evaluate(ev_t);
      ImVec2 cur = to_screen(t, v);
      dl->AddLine(prev, cur, col_line, 2.f);
      prev = cur;
    }
  }

  ImVec2 mouse = ImGui::GetMousePos();
  int hover_idx = -1;
  float point_r = 7.f;
  for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
    ImVec2 sp = to_screen(pts[i].time_sec, pts[i].value);
    float dx = mouse.x - sp.x, dy = mouse.y - sp.y;
    if (dx * dx + dy * dy < point_r * point_r * 4) {
      hover_idx = i;
      break;
    }
  }

  if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
    if (ed.drag_idx >= 0 && ed.drag_idx < static_cast<int>(pts.size())) {
      auto [t, v] = from_screen(mouse);
      pts[ed.drag_idx].time_sec = t;
      pts[ed.drag_idx].value = v;
    }
  }
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && hovered) {
    if (hover_idx >= 0) {
      ed.drag_idx = hover_idx;
      ed.dragging = true;
      ed.frozen_t_max = t_max_d;
      ed.frozen_v_min = v_min_d;
      ed.frozen_v_max = v_max_d;
    } else if (mouse.x >= px0 && mouse.x <= px1 && mouse.y >= py0 &&
               mouse.y <= py1) {
      auto [t, v] = from_screen(mouse);
      pts.push_back({t, v});
      std::sort(pts.begin(), pts.end(),
                [](const table_point& a, const table_point& b) {
                  return a.time_sec < b.time_sec;
                });
      ed.drag_idx = -1;
    }
  }
  if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && ed.drag_idx >= 0) {
    std::sort(pts.begin(), pts.end(),
              [](const table_point& a, const table_point& b) {
                return a.time_sec < b.time_sec;
              });
    ed.drag_idx = -1;
    ed.dragging = false;
  }
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && hovered &&
      hover_idx >= 0) {
    pts.erase(pts.begin() + hover_idx);
    ed.drag_idx = -1;
  }

  for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
    ImVec2 sp = to_screen(pts[i].time_sec, pts[i].value);
    ImU32 col = (i == hover_idx || i == ed.drag_idx) ? col_point_hl : col_point;
    if (pts[i].hold) {
      float r = point_r * 0.85f;
      dl->AddRectFilled({sp.x - r, sp.y - r}, {sp.x + r, sp.y + r}, col);
      dl->AddRect({sp.x - r, sp.y - r}, {sp.x + r, sp.y + r},
                  IM_COL32(0, 0, 0, 200), 0, 0, 1.5f);
    } else {
      dl->AddCircleFilled(sp, point_r, col);
      dl->AddCircle(sp, point_r, IM_COL32(0, 0, 0, 200), 0, 1.5f);
    }
  }

  if (hovered && hover_idx >= 0) {
    ImGui::SetTooltip("t=%.2fs  val=%.3f\nDrag to move, right-click to delete",
                      pts[hover_idx].time_sec, pts[hover_idx].value);
  } else if (hovered && mouse.x >= px0 && mouse.y >= py0 && mouse.y <= py1) {
    auto [t, v] = from_screen(mouse);
    ImGui::SetTooltip("Click to add point\nt=%.2fs  val=%.1f", t, v);
  }
}

inline void draw_source_editor(app_state& state, source_editor_state& ed) {
  if (!ed.open) return;

  signal_source* src_ptr = nullptr;
  std::string window_title;
  state.tx_sched.with_jobs([&](std::vector<tx_job>& jobs) {
    for (auto& job : jobs) {
      if (job.instance_id == ed.job_id) {
        auto it = job.signal_sources.find(ed.signal_name);
        if (it != job.signal_sources.end()) {
          src_ptr = &it->second;
          window_title = std::format("Source: {} [0x{:03X} {}]###srceditor",
                                     ed.signal_name, job.msg_id, job.msg_name);
        }
        break;
      }
    }
  });

  if (!src_ptr) {
    ed.open = false;
    return;
  }
  auto& src = *src_ptr;

  ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin(window_title.c_str(), &ed.open)) {
    ImGui::End();
    return;
  }

  int force = ed.pending_tab;
  auto tab_flag = [&](source_mode m) -> ImGuiTabItemFlags {
    return (force == static_cast<int>(m)) ? ImGuiTabItemFlags_SetSelected : 0;
  };
  if (ImGui::BeginTabBar("##modes")) {
    if (ImGui::BeginTabItem("Waveform", nullptr,
                            tab_flag(source_mode::waveform))) {
      src.mode = source_mode::waveform;
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Table", nullptr, tab_flag(source_mode::table))) {
      src.mode = source_mode::table;
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Expression", nullptr,
                            tab_flag(source_mode::expression))) {
      src.mode = source_mode::expression;
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Constant", nullptr,
                            tab_flag(source_mode::constant))) {
      src.mode = source_mode::constant;
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ed.pending_tab = -1;

  ImGui::Separator();

  if (src.mode == source_mode::waveform) {
    static const char* wave_names[] = {"Sine", "Ramp", "Square", "Triangle"};
    int wt = static_cast<int>(src.waveform.type);
    ImGui::SetNextItemWidth(150.f);
    if (ImGui::Combo("Type", &wt, wave_names, 4))
      src.waveform.type = static_cast<waveform_type>(wt);
    ImGui::SetNextItemWidth(150.f);
    ImGui::InputDouble("Min", &src.waveform.min_val, 0, 0, "%.3f");
    ImGui::SetNextItemWidth(150.f);
    ImGui::InputDouble("Max", &src.waveform.max_val, 0, 0, "%.3f");
    ImGui::SetNextItemWidth(150.f);
    ImGui::InputDouble("Period (s)", &src.waveform.period_sec, 0, 0, "%.3f");
    src.waveform.period_sec = std::max(0.001, src.waveform.period_sec);
    jcan::checkbox("Repeat", &src.repeat);

    ImGui::Spacing();
    float preview[256];
    src.preview(preview, 256, src.preview_duration());
    ImGui::PlotLines("##preview", preview, 256, 0, nullptr,
                     static_cast<float>(src.waveform.min_val),
                     static_cast<float>(src.waveform.max_val),
                     ImVec2(ImGui::GetContentRegionAvail().x,
                            ImGui::GetContentRegionAvail().y));
  }

  else if (src.mode == source_mode::table) {
    auto& pts = src.table.points;

    jcan::checkbox("Repeat", &src.repeat);
    ImGui::SameLine();
    if (ImGui::SmallButton("Export CSV")) {
      nfdchar_t* out_path = nullptr;
      nfdfilteritem_t filters[] = {{"CSV", "csv"}};
      if (NFD_SaveDialog(&out_path, filters, 1, nullptr,
                          "signal_table.csv") == NFD_OKAY) {
        std::ofstream ofs(out_path);
        if (ofs.is_open()) {
          ofs << "time,value\n";
          for (const auto& p : pts)
            ofs << std::format("{:.6f},{:.6f}\n", p.time_sec, p.value);
        }
        NFD_FreePath(out_path);
      }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Import CSV")) {
      nfdchar_t* out_path = nullptr;
      nfdfilteritem_t filters[] = {{"CSV", "csv"}};
      if (NFD_OpenDialog(&out_path, filters, 1, nullptr) == NFD_OKAY) {
        std::ifstream ifs(out_path);
        if (ifs.is_open()) {
          pts.clear();
          std::string line;
          std::getline(ifs, line);
          while (std::getline(ifs, line)) {
            auto comma = line.find(',');
            if (comma == std::string::npos) continue;
            try {
              double t = std::stod(line.substr(0, comma));
              double v = std::stod(line.substr(comma + 1));
              pts.push_back({t, v, true});
            } catch (...) {}
          }
          std::sort(pts.begin(), pts.end(),
                    [](const table_point& a, const table_point& b) {
                      return a.time_sec < b.time_sec;
                    });
        }
        NFD_FreePath(out_path);
      }
    }

    float avail_w = ImGui::GetContentRegionAvail().x;
    float avail_h = ImGui::GetContentRegionAvail().y;
    float list_w = std::min(230.f, avail_w * 0.4f);

    if (ImGui::BeginChild("##ptlist", ImVec2(list_w, avail_h), true)) {
      ImGui::TextDisabled("Time    Value");
      ImGui::Separator();

      int del_idx = -1;
      for (int pi = 0; pi < static_cast<int>(pts.size()); ++pi) {
        ImGui::PushID(pi);
        ImGui::SetNextItemWidth(50.f);
        ImGui::InputDouble("##t", &pts[pi].time_sec, 0, 0, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(55.f);
        ImGui::InputDouble("##v", &pts[pi].value, 0, 0, "%.2f");
        ImGui::SameLine();
        bool h = pts[pi].hold;
        if (ImGui::SmallButton(h ? "H" : "~")) pts[pi].hold = !h;
        if (ImGui::IsItemHovered())
          ImGui::SetTooltip(h ? "Hold (step) - click for interpolate"
                              : "Interpolate (ramp) - click for hold");
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) del_idx = pi;
        ImGui::PopID();
      }
      if (del_idx >= 0) pts.erase(pts.begin() + del_idx);

      ImGui::Spacing();
      if (ImGui::Button("+ Add Point", ImVec2(-1, 0))) {
        double new_t = pts.empty() ? 0.0 : pts.back().time_sec + 1.0;
        pts.push_back({new_t, 0.0, true});
      }

      if (!ImGui::IsAnyItemActive()) {
        std::sort(pts.begin(), pts.end(),
                  [](const table_point& a, const table_point& b) {
                    return a.time_sec < b.time_sec;
                  });
      }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    if (ImGui::BeginChild("##ptchart", ImVec2(0, avail_h), false)) {
      float cw = ImGui::GetContentRegionAvail().x;
      float ch = ImGui::GetContentRegionAvail().y;
      if (cw > 60.f && ch > 40.f) draw_table_chart(src, cw, ch, ed, state.colors);
    }
    ImGui::EndChild();
  }

  else if (src.mode == source_mode::expression) {
    char buf[256]{};
    std::strncpy(buf, src.expression.text.c_str(), sizeof(buf) - 1);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputText("##expr", buf, sizeof(buf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
      src.expression.text = buf;
      src.expression.compile();
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      src.expression.text = buf;
      src.expression.compile();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Variables: t (elapsed seconds)");
    ImGui::TextDisabled("Constants: pi, e");
    ImGui::TextDisabled("Functions: sin cos abs sqrt min max pow clamp");
    ImGui::Spacing();
    ImGui::TextDisabled("Examples:");
    ImGui::BulletText("sin(t * 2 * pi) * 100 + 200");
    ImGui::BulletText("t * 50");
    ImGui::BulletText("clamp(t * 10, 0, 255)");
    ImGui::BulletText("min(t^2, 1000)");
    ImGui::BulletText("150 + 50 * sin(t) * cos(t * 0.3)");

    if (!src.expression.error.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(state.colors.error_text, "%s",
                         src.expression.error.c_str());
    } else if (src.expression.ast_) {
      ImGui::Spacing();
      float preview[256];
      src.preview(preview, 256, src.preview_duration());
      float vmin = preview[0], vmax = preview[0];
      for (int i = 1; i < 256; ++i) {
        vmin = std::min(vmin, preview[i]);
        vmax = std::max(vmax, preview[i]);
      }
      if (vmin == vmax) {
        vmin -= 1.f;
        vmax += 1.f;
      }
      ImGui::PlotLines("##preview", preview, 256, 0, nullptr, vmin, vmax,
                       ImVec2(ImGui::GetContentRegionAvail().x,
                              ImGui::GetContentRegionAvail().y));
    }
  }

  else if (src.mode == source_mode::constant) {
    ImGui::TextWrapped(
        "Using the constant value from the slider in the Transmitter panel.");
    if (ImGui::Button("Close")) ed.open = false;
  }

  ImGui::End();
}

inline void draw_transmitter(app_state& state) {
  static source_editor_state src_editor;

  draw_source_editor(state, src_editor);

  ImGui::SetNextWindowSize(ImVec2(520, 500), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Transmitter")) {
    ImGui::End();
    return;
  }

  if (!state.connected) {
    ImGui::TextWrapped("Connect to a CAN adapter first.");
    ImGui::End();
    return;
  }

  if (state.any_dbc_loaded()) {
    auto msg_ids = state.all_message_ids();
    static int selected_idx = 0;
    static char tx_filter[64]{};

    std::vector<std::string> labels;
    labels.reserve(msg_ids.size());
    for (auto mid : msg_ids) {
      auto name = state.any_message_name(mid);
      labels.push_back(std::format("0x{:03X} {}", mid, name));
    }

    std::vector<int> filtered;
    {
      std::string filt(tx_filter);
      for (auto& c : filt) c = static_cast<char>(std::toupper(c));
      for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        if (filt.empty()) {
          filtered.push_back(i);
        } else {
          std::string upper_label = labels[i];
          for (auto& c : upper_label) c = static_cast<char>(std::toupper(c));
          if (upper_label.find(filt) != std::string::npos)
            filtered.push_back(i);
        }
      }
    }

    const char* preview =
        (selected_idx >= 0 && selected_idx < static_cast<int>(labels.size()))
            ? labels[selected_idx].c_str()
            : "Select DBC message...";

    float combo_w = ImGui::CalcTextSize(preview).x +
                    ImGui::GetStyle().FramePadding.x * 2.f + 30.f;
    combo_w = std::min(combo_w, ImGui::GetContentRegionAvail().x - 120.f);
    ImGui::SetNextItemWidth(combo_w);
    if (ImGui::BeginCombo("##msg_select", preview)) {
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
      if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
      ImGui::InputTextWithHint("##tx_filter", "Type to filter...", tx_filter,
                               sizeof(tx_filter));

      for (int fi : filtered) {
        bool is_selected = (fi == selected_idx);
        if (ImGui::Selectable(labels[fi].c_str(), is_selected)) {
          selected_idx = fi;
        }
        if (is_selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    ImGui::SameLine();
    if (ImGui::Button("Add DBC msg") &&
        selected_idx < static_cast<int>(msg_ids.size())) {
      auto mid = msg_ids[selected_idx];
      const auto& eng = state.dbc_for_id(mid);
      auto name = eng.message_name(mid);

      tx_job job;
      job.instance_id = tx_job::next_id();
      job.msg_id = mid;
      job.msg_name = name;
      job.is_raw = false;
      job.frame.id = mid;
      job.frame.extended = (mid > 0x7FF);
      job.frame.dlc = eng.message_dlc(mid);
      std::memset(job.frame.data.data(), 0, 64);
      auto sigs = eng.signal_infos(mid);
      for (const auto& si : sigs) {
        signal_source src;
        src.constant_value = 0.0;
        src.waveform.min_val = si.minimum;
        src.waveform.max_val = si.maximum;
        if (src.waveform.min_val == src.waveform.max_val) {
          src.waveform.max_val = 1.0;
        }
        job.signal_sources[si.name] = std::move(src);
      }
      state.tx_sched.upsert(std::move(job));
    }
    ImGui::SameLine();
  }

  {
    static uint32_t custom_id = 0x100;
    static int custom_dlc = 8;

    if (ImGui::Button("Add Custom")) {
      uint32_t key = custom_id | 0x80000000u;

      tx_job job;
      job.instance_id = tx_job::next_id();
      job.msg_id = key;
      job.msg_name = std::format("Custom 0x{:03X}", custom_id);
      job.is_raw = true;
      job.frame.id = custom_id;
      job.frame.extended = (custom_id > 0x7FF);
      job.frame.dlc = static_cast<uint8_t>(custom_dlc);
      std::memset(job.frame.data.data(), 0, 64);
      state.tx_sched.upsert(std::move(job));
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputScalar("##custom_id", ImGuiDataType_U32, &custom_id, nullptr,
                       nullptr, "%03X", ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::Text("DLC:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(40);
    ImGui::InputInt("##custom_dlc", &custom_dlc, 0, 0);
    custom_dlc = std::clamp(custom_dlc, 0, 8);
  }

  ImGui::Separator();

  state.tx_sched.with_jobs([&](std::vector<tx_job>& jobs) {
    if (jobs.empty()) {
      ImGui::TextDisabled("No messages in TX list.");
      return;
    }

    int remove_idx = -1;
    for (int ji = 0; ji < static_cast<int>(jobs.size()); ++ji) {
      auto& job = jobs[ji];
      ImGui::PushID(ji);

      auto header = std::format("0x{:03X} {} [{:.0f}ms]###txjob_{}",
                                job.frame.id, job.msg_name, job.period_ms, ji);

      bool open = ImGui::CollapsingHeader(header.c_str(),
                                          ImGuiTreeNodeFlags_DefaultOpen);

      if (open) {
        if (job.enabled) {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.2f, 0.2f, 1.0f));
          if (ImGui::Button("Stop")) job.enabled = false;
          ImGui::PopStyleColor(2);
        } else {
          ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.15f, 1.0f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
          if (ImGui::Button("Start")) job.enabled = true;
          ImGui::PopStyleColor(2);
        }
        if (job.enabled && !job.was_enabled)
          job.start_time = tx_job::clock::now();
        job.was_enabled = job.enabled;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("Period (ms)", &job.period_ms, 1.0f, 1.0f, 10000.f,
                         "%.0f");
        ImGui::SameLine();
        if (ImGui::Button("Send Once")) {
          if (!job.is_raw && state.any_dbc_loaded()) {
            auto vals = job.evaluate_signals();
            job.frame = state.dbc_for_id(job.msg_id).encode(job.msg_id, vals);
          }
          if (auto* a = state.tx_adapter()) (void)adapter_send(*a, job.frame);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove")) {
          remove_idx = ji;
        }

        ImGui::Indent(10.f);

        if (job.is_raw) {
          uint32_t edit_id = job.frame.id;
          ImGui::Text("ID:");
          ImGui::SameLine();
          ImGui::SetNextItemWidth(80);
          if (ImGui::InputScalar("##raw_id", ImGuiDataType_U32, &edit_id,
                                 nullptr, nullptr, "%03X",
                                 ImGuiInputTextFlags_CharsHexadecimal)) {
            job.frame.id = edit_id;
            job.frame.extended = (edit_id > 0x7FF);
            job.msg_name = std::format("Custom 0x{:03X}", edit_id);
          }
          ImGui::SameLine();
          ImGui::Text("DLC:");
          ImGui::SameLine();
          int dlc_edit = job.frame.dlc;
          ImGui::SetNextItemWidth(40);
          if (ImGui::InputInt("##raw_dlc", &dlc_edit, 0, 0)) {
            int max_dlc = job.frame.fd ? 15 : 8;
            job.frame.dlc =
                static_cast<uint8_t>(std::clamp(dlc_edit, 0, max_dlc));
          }
          ImGui::SameLine();
          jcan::checkbox("Ext", &job.frame.extended);
          ImGui::SameLine();
          jcan::checkbox("FD", &job.frame.fd);

          uint8_t payload_len = frame_payload_len(job.frame);
          ImGui::Text("Data:");
          ImGui::SameLine();
          if (state.mono_font) ImGui::PushFont(state.mono_font);
          for (uint8_t bi = 0; bi < payload_len; ++bi) {
            if (bi) ImGui::SameLine(0, 4);
            ImGui::PushID(bi);
            ImGui::SetNextItemWidth(30);
            ImGui::InputScalar("##byte", ImGuiDataType_U8, &job.frame.data[bi],
                               nullptr, nullptr, "%02X",
                               ImGuiInputTextFlags_CharsHexadecimal);
            ImGui::PopID();
          }
          if (state.mono_font) ImGui::PopFont();
        } else {
          const auto& eng = state.dbc_for_id(job.msg_id);
          auto sigs = eng.signal_infos(job.msg_id);
          ImGui::TextDisabled("Right-click signals for waveform/table/expression sources");

          float label_w = 0.f;
          for (const auto& si : sigs) {
            auto lbl = si.unit.empty()
                           ? si.name
                           : std::format("{} ({})", si.name, si.unit);
            float w = ImGui::CalcTextSize(lbl.c_str()).x;
            if (w > label_w) label_w = w;
          }
          label_w += ImGui::GetStyle().ItemSpacing.x;

          if (ImGui::BeginTable("##sigs", 2, ImGuiTableFlags_None)) {
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed,
                                    label_w);
            ImGui::TableSetupColumn("ctrl", ImGuiTableColumnFlags_WidthStretch);

            for (const auto& si : sigs) {
              auto& src = job.signal_sources[si.name];

              float fmin = static_cast<float>(si.minimum);
              float fmax = static_cast<float>(si.maximum);
              if (fmin == fmax) {
                fmin = si.is_signed
                           ? -static_cast<float>(1ULL << (si.bit_size - 1))
                           : 0.f;
                fmax = si.is_signed
                           ? static_cast<float>((1ULL << (si.bit_size - 1)) - 1)
                           : static_cast<float>((1ULL << si.bit_size) - 1);
                fmin = fmin * static_cast<float>(si.factor) +
                       static_cast<float>(si.offset);
                fmax = fmax * static_cast<float>(si.factor) +
                       static_cast<float>(si.offset);
                if (fmin > fmax) std::swap(fmin, fmax);
              }

              auto label = si.unit.empty()
                               ? si.name
                               : std::format("{} ({})", si.name, si.unit);

              bool is_bool =
                  (si.bit_size == 1 && si.factor == 1.0 && si.offset == 0.0);
              bool is_integer = !is_bool &&
                                std::floor(si.factor) == si.factor &&
                                std::floor(si.offset) == si.offset;

              ImGui::TableNextRow();
              ImGui::TableNextColumn();
              ImGui::AlignTextToFramePadding();

              bool has_source = src.mode != source_mode::constant;
              if (has_source)
                ImGui::TextColored(state.colors.active_source_label, "%s",
                                   label.c_str());
              else
                ImGui::TextUnformatted(label.c_str());

              ImGui::TableNextColumn();

              if (src.mode == source_mode::constant) {
                auto& val = src.constant_value;
                auto slider_id = std::format("##sig_{}", si.name);

                if (is_bool) {
                  bool bval = (val != 0.0);
                  if (jcan::checkbox(slider_id.c_str(), &bval))
                    val = bval ? 1.0 : 0.0;
                } else if (is_integer) {
                  int ival = static_cast<int>(val);
                  int imin = static_cast<int>(fmin);
                  int imax = static_cast<int>(fmax);
                  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x -
                                          100.f);
                  if (ImGui::SliderInt(slider_id.c_str(), &ival, imin, imax))
                    val = static_cast<double>(ival);
                } else {
                  float fval = static_cast<float>(val);
                  ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x -
                                          100.f);
                  if (ImGui::SliderFloat(slider_id.c_str(), &fval, fmin, fmax,
                                         "%.3f"))
                    val = static_cast<double>(fval);
                }

                if (!is_bool) {
                  ImGui::SameLine();
                  auto raw_factor = (si.factor != 0.0) ? si.factor : 1.0;
                  auto raw_val =
                      static_cast<int64_t>((val - si.offset) / raw_factor);
                  auto raw_id = std::format("##raw_{}", si.name);
                  ImGui::SetNextItemWidth(90.f);
                  if (ImGui::InputScalar(raw_id.c_str(), ImGuiDataType_S64,
                                         &raw_val))
                    val = static_cast<double>(raw_val) * si.factor + si.offset;
                }
              } else {
                double cur = src.evaluate(job.elapsed_sec());
                float cur_f = static_cast<float>(cur);
                float fraction =
                    (fmax != fmin)
                        ? std::clamp((cur_f - fmin) / (fmax - fmin), 0.f, 1.f)
                        : 0.5f;
                auto overlay = std::format("{:.3f}", cur);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::ProgressBar(fraction, ImVec2(0, 0), overlay.c_str());
              }

              if (ImGui::BeginPopupContextItem(
                      std::format("##ctx_{}", si.name).c_str())) {
                ImGui::TextDisabled("%s", label.c_str());
                ImGui::Separator();

                auto open_editor = [&](source_mode m) {
                  src.mode = m;
                  src_editor.open = true;
                  src_editor.pending_tab = static_cast<int>(src.mode);
                  src_editor.job_id = job.instance_id;
                  src_editor.signal_name = si.name;
                };

                if (ImGui::MenuItem("Constant", nullptr,
                                    src.mode == source_mode::constant))
                  src.mode = source_mode::constant;
                if (ImGui::MenuItem("Waveform...", nullptr,
                                    src.mode == source_mode::waveform))
                  open_editor(source_mode::waveform);
                if (ImGui::MenuItem("Table...", nullptr,
                                    src.mode == source_mode::table))
                  open_editor(source_mode::table);
                if (ImGui::MenuItem("Expression...", nullptr,
                                    src.mode == source_mode::expression))
                  open_editor(source_mode::expression);

                if (src.mode != source_mode::constant) {
                  ImGui::Separator();
                  if (ImGui::MenuItem("Edit Source...")) {
                    src_editor.open = true;
                    src_editor.pending_tab = static_cast<int>(src.mode);
                    src_editor.job_id = job.instance_id;
                    src_editor.signal_name = si.name;
                  }
                }

                ImGui::EndPopup();
              }
            }

            ImGui::EndTable();
          }

          auto vals = job.evaluate_signals();
          job.frame = eng.encode(job.msg_id, vals);
        }

        ImGui::TextDisabled("  Frame: ");
        ImGui::SameLine();
        if (state.mono_font) ImGui::PushFont(state.mono_font);
        uint8_t show_len = frame_payload_len(job.frame);
        for (uint8_t bi = 0; bi < show_len; ++bi) {
          if (bi) ImGui::SameLine(0, 2);
          ImGui::Text("%02X", job.frame.data[bi]);
        }
        if (state.mono_font) ImGui::PopFont();

        ImGui::Unindent(10.f);
        ImGui::Spacing();
      }

      ImGui::PopID();
    }

    if (remove_idx >= 0) jobs.erase(jobs.begin() + remove_idx);
  });

  ImGui::End();
}

}  // namespace jcan::widgets
