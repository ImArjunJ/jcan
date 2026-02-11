#pragma once

#include <imgui.h>

#include <algorithm>
#include <format>
#include <string>
#include <vector>

#include "signal_store.hpp"

namespace jcan::widgets {

/// Persistent state for the channel list sidebar.
struct channel_list_state {
  char filter[128]{};
};

/// Draw the channel list sidebar.
/// `is_on_chart` should return true if the signal is currently plotted.
inline void draw_channel_list(
    channel_list_state& cl, const signal_store& store,
    const std::function<std::string(uint32_t)>& dbc_msg_name_fn,
    const std::function<bool(const signal_key&)>& is_on_chart) {
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##ch_filter", "Search channels...", cl.filter,
                           sizeof(cl.filter));

  auto channels = store.all_channels();

  // Filter
  std::string filt_upper;
  if (cl.filter[0] != '\0') {
    filt_upper = cl.filter;
    for (auto& c : filt_upper) c = static_cast<char>(std::toupper(c));
  }

  std::vector<const channel_info*> visible;
  visible.reserve(channels.size());
  for (const auto* ch : channels) {
    if (filt_upper.empty()) {
      visible.push_back(ch);
      continue;
    }
    auto upper = [](std::string s) {
      for (auto& c : s) c = static_cast<char>(std::toupper(c));
      return s;
    };
    if (upper(ch->key.name).find(filt_upper) != std::string::npos ||
        upper(ch->unit).find(filt_upper) != std::string::npos) {
      visible.push_back(ch);
      continue;
    }
    auto id_str = std::format("{:03X}", ch->key.msg_id);
    if (id_str.find(filt_upper) != std::string::npos) {
      visible.push_back(ch);
      continue;
    }
    auto msg_name_str = dbc_msg_name_fn(ch->key.msg_id);
    if (upper(msg_name_str).find(filt_upper) != std::string::npos) {
      visible.push_back(ch);
    }
  }

  ImGui::Text("%zu / %zu channels", visible.size(), channels.size());
  ImGui::Separator();

  if (ImGui::BeginChild("##ch_list", ImVec2(0, 0), ImGuiChildFlags_None,
                         ImGuiWindowFlags_None)) {
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible.size()));
    while (clipper.Step()) {
      for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
        const auto* ch = visible[static_cast<std::size_t>(i)];
        bool on = is_on_chart(ch->key);

        ImGui::PushID(i);

        // Colored bullet if on chart
        if (on) {
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
        }

        // Selectable row (no click action — drag only)
        ImGui::Selectable("##sel", on, ImGuiSelectableFlags_SpanAllColumns);

        // Drag source — carry a pointer to the signal_key
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
          const signal_key* ptr = &ch->key;
          ImGui::SetDragDropPayload("SIGNAL_KEY", &ptr, sizeof(ptr));
          ImGui::Text("%s", ch->key.name.c_str());
          ImGui::EndDragDropSource();
        }

        ImGui::SameLine();

        // Signal name + value
        ImGui::Text("%s", ch->key.name.c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 90);
        ImGui::TextDisabled("%.4g%s", ch->last_value,
                            ch->unit.empty() ? "" : (" " + ch->unit).c_str());

        if (on) {
          ImGui::PopStyleColor();
        }

        // Tooltip with details
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
          ImGui::BeginTooltip();
          auto msg_name = dbc_msg_name_fn(ch->key.msg_id);
          ImGui::Text("Message: %s (0x%03X)", msg_name.c_str(),
                      ch->key.msg_id);
          ImGui::Text("Signal: %s", ch->key.name.c_str());
          ImGui::Text("Value: %.6g %s", ch->last_value, ch->unit.c_str());
          if (ch->minimum != ch->maximum) {
            ImGui::Text("Range: [%.4g .. %.4g]", ch->minimum, ch->maximum);
          }
          ImGui::EndTooltip();
        }

        ImGui::PopID();
      }
    }
  }
  ImGui::EndChild();
}

}  // namespace jcan::widgets
