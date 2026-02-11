#pragma once

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <string>
#include <vector>

#include "app_state.hpp"

namespace jcan::widgets {

inline void draw_transmitter(app_state& state) {
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

  if (state.dbc.loaded()) {
    auto msg_ids = state.dbc.message_ids();
    static int selected_idx = 0;
    static char tx_filter[64]{};

    std::vector<std::string> labels;
    labels.reserve(msg_ids.size());
    for (auto mid : msg_ids) {
      auto name = state.dbc.message_name(mid);
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
      auto name = state.dbc.message_name(mid);

      bool exists = false;
      state.tx_sched.with_jobs([&](auto& jobs) {
        for (auto& j : jobs)
          if (j.msg_id == mid && !j.is_raw) {
            exists = true;
            break;
          }
      });

      if (!exists) {
        tx_job job;
        job.msg_id = mid;
        job.msg_name = name;
        job.is_raw = false;
        job.frame.id = mid;
        job.frame.extended = (mid > 0x7FF);
        job.frame.dlc = state.dbc.message_dlc(mid);
        std::memset(job.frame.data.data(), 0, 64);
        auto sigs = state.dbc.signal_infos(mid);
        for (const auto& si : sigs) job.signal_values[si.name] = 0.0;
        state.tx_sched.upsert(std::move(job));
      }
    }
    ImGui::SameLine();
  }

  {
    static uint32_t custom_id = 0x100;
    static int custom_dlc = 8;

    if (ImGui::Button("Add Custom")) {
      uint32_t key = custom_id | 0x80000000u;

      bool exists = false;
      state.tx_sched.with_jobs([&](auto& jobs) {
        for (auto& j : jobs)
          if (j.msg_id == key) {
            exists = true;
            break;
          }
      });

      if (!exists) {
        tx_job job;
        job.msg_id = key;
        job.msg_name = std::format("Custom 0x{:03X}", custom_id);
        job.is_raw = true;
        job.frame.id = custom_id;
        job.frame.extended = (custom_id > 0x7FF);
        job.frame.dlc = static_cast<uint8_t>(custom_dlc);
        std::memset(job.frame.data.data(), 0, 64);
        state.tx_sched.upsert(std::move(job));
      }
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
        ImGui::Checkbox("Enable TX", &job.enabled);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        ImGui::DragFloat("Period (ms)", &job.period_ms, 1.0f, 1.0f, 10000.f,
                         "%.0f");
        ImGui::SameLine();
        if (ImGui::Button("Send Once")) {
          if (!job.is_raw && state.dbc.loaded())
            job.frame = state.dbc.encode(job.msg_id, job.signal_values);
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
          ImGui::Checkbox("Ext", &job.frame.extended);
          ImGui::SameLine();
          ImGui::Checkbox("FD", &job.frame.fd);

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
          auto sigs = state.dbc.signal_infos(job.msg_id);

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
              auto& val = job.signal_values[si.name];

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
              ImGui::TextUnformatted(label.c_str());

              ImGui::TableNextColumn();
              auto slider_id = std::format("##sig_{}", si.name);

              if (is_bool) {
                bool bval = (val != 0.0);
                if (ImGui::Checkbox(slider_id.c_str(), &bval))
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
            }

            ImGui::EndTable();
          }

          job.frame = state.dbc.encode(job.msg_id, job.signal_values);
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
