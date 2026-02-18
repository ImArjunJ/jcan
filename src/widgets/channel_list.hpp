#pragma once

#include <imgui.h>

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "signal_store.hpp"
#include "strip_chart.hpp"

namespace jcan::widgets {

struct channel_list_state {
  char filter[128]{};
  std::optional<signal_drag_payload> active_drag;
};

struct overlay_store_info {
  int layer_idx{0};
  const signal_store* store{nullptr};
  std::string name;
};

inline void draw_channel_list(
    channel_list_state& cl, const signal_store& primary_store,
    const std::vector<overlay_store_info>& overlay_stores,
    const std::function<std::string(uint32_t)>& dbc_msg_name_fn,
    const std::function<bool(const signal_key&, int)>& is_on_chart,
    ImVec4 on_chart_color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f)) {
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##ch_filter", "Search channels...", cl.filter,
                           sizeof(cl.filter));

  struct entry {
    const channel_info* info;
    int layer_idx;
    std::string prefix;
  };

  std::string filt_upper;
  if (cl.filter[0] != '\0') {
    filt_upper = cl.filter;
    for (auto& c : filt_upper) c = static_cast<char>(std::toupper(c));
  }

  auto upper = [](std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(c));
    return s;
  };

  auto matches = [&](const channel_info* ch) -> bool {
    if (filt_upper.empty()) return true;
    if (upper(ch->key.name).find(filt_upper) != std::string::npos) return true;
    if (upper(ch->unit).find(filt_upper) != std::string::npos) return true;
    auto id_str = std::format("{:03X}", ch->key.msg_id);
    if (id_str.find(filt_upper) != std::string::npos) return true;
    auto msg_name = dbc_msg_name_fn(ch->key.msg_id);
    if (upper(msg_name).find(filt_upper) != std::string::npos) return true;
    return false;
  };

  std::vector<entry> visible;
  std::size_t total = 0;

  auto primary_channels = primary_store.all_channels();
  total += primary_channels.size();
  for (const auto* ch : primary_channels)
    if (matches(ch)) visible.push_back({ch, -1, {}});

  for (const auto& ov : overlay_stores) {
    if (!ov.store) continue;
    auto ov_channels = ov.store->all_channels();
    total += ov_channels.size();
    for (const auto* ch : ov_channels)
      if (matches(ch)) visible.push_back({ch, ov.layer_idx, ov.name});
  }

  ImGui::Text("%zu / %zu channels", visible.size(), total);
  if (visible.empty() && total == 0)
    ImGui::TextDisabled("Load a DBC file to see signals");
  else if (!visible.empty())
    ImGui::TextDisabled("Drag signals to a chart to plot");
  ImGui::Separator();

  if (ImGui::BeginChild("##ch_list", ImVec2(0, 0), ImGuiChildFlags_None,
                        ImGuiWindowFlags_None)) {
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible.size()));
    while (clipper.Step()) {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
        const auto& e = visible[static_cast<std::size_t>(i)];
        bool on = is_on_chart(e.info->key, e.layer_idx);

        ImGui::PushID(i);

        if (on)
          ImGui::PushStyleColor(ImGuiCol_Text, on_chart_color);

        ImGui::Selectable("##sel", on, ImGuiSelectableFlags_SpanAllColumns);
        bool row_hovered = ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip);

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
          cl.active_drag = signal_drag_payload{e.info->key, e.layer_idx};
          const signal_drag_payload* ptr = &*cl.active_drag;
          ImGui::SetDragDropPayload("SIGNAL_DRAG", &ptr, sizeof(ptr));
          if (!e.prefix.empty())
            ImGui::Text("[%s] %s", e.prefix.c_str(), e.info->key.name.c_str());
          else
            ImGui::Text("%s", e.info->key.name.c_str());
          ImGui::EndDragDropSource();
        }

        ImGui::SameLine();
        if (!e.prefix.empty()) {
          ImGui::TextDisabled("[%s]", e.prefix.c_str());
          ImGui::SameLine(0, 4);
        }
        ImGui::TextUnformatted(e.info->key.name.c_str());

        if (on)
          ImGui::PopStyleColor();

        if (row_hovered) {
          ImGui::BeginTooltip();
          if (!e.prefix.empty())
            ImGui::TextDisabled("Layer: %s", e.prefix.c_str());
          auto msg_name = dbc_msg_name_fn(e.info->key.msg_id);
          ImGui::Text("Message: %s (0x%03X)", msg_name.c_str(),
                      e.info->key.msg_id);
          ImGui::Text("Signal: %s", e.info->key.name.c_str());
          ImGui::Text("Value: %.6g %s", e.info->last_value,
                      e.info->unit.c_str());
          if (e.info->minimum != e.info->maximum)
            ImGui::Text("Range: [%.4g .. %.4g]", e.info->minimum,
                        e.info->maximum);
          ImGui::EndTooltip();
        }

        ImGui::PopID();
      }
    }
  }
  ImGui::EndChild();
}

}  // namespace jcan::widgets
