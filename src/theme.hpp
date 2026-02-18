#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <cstdint>
#include <string_view>

namespace jcan {

enum class theme_id : int {
  dark_flat = 0,
  midnight,
  automotive,
  light,
  count_,
};

[[nodiscard]] inline std::string_view theme_name(theme_id id) {
  switch (id) {
    case theme_id::dark_flat:   return "Dark Flat";
    case theme_id::midnight:    return "Midnight";
    case theme_id::automotive:  return "Automotive";
    case theme_id::light:       return "Light";
    default:                    return "Unknown";
  }
}

struct semantic_colors {
  ImVec4 status_connected;
  ImVec4 status_disconnected;
  ImVec4 status_recording;

  ImVec4 byte_changed;
  ImVec4 new_frame_row_bg;

  ImVec4 error_text;
  ImVec4 active_source_label;

  ImVec4 load_ok;
  ImVec4 load_warn;
  ImVec4 load_critical;

  ImVec4 live_button;
  ImVec4 paused_button;
  ImVec4 active_chart_header;

  ImU32 chart_bg;
  ImU32 chart_border;
  ImU32 chart_grid;
  ImU32 chart_grid_text;
  ImU32 chart_cursor;

  ImU32 editor_bg;
  ImU32 editor_grid;
  ImU32 editor_axis;
  ImU32 editor_line;
  ImU32 editor_point;
  ImU32 editor_point_hl;
  ImU32 editor_text;

  ImVec4 channel_on_chart;

  ImVec4 clear_color;
};

namespace detail {

inline semantic_colors apply_dark_flat() {
  auto& s = ImGui::GetStyle();
  auto* c = s.Colors;

  ImVec4 bg        {0.11f, 0.11f, 0.13f, 1.00f};
  ImVec4 bg_dark   {0.08f, 0.08f, 0.10f, 1.00f};
  ImVec4 bg_light  {0.16f, 0.16f, 0.18f, 1.00f};
  ImVec4 accent    {0.22f, 0.52f, 0.62f, 1.00f};
  ImVec4 accent_dim{0.16f, 0.38f, 0.46f, 1.00f};
  ImVec4 accent_hi {0.28f, 0.62f, 0.72f, 1.00f};
  ImVec4 text      {0.86f, 0.86f, 0.86f, 1.00f};
  ImVec4 text_dim  {0.50f, 0.50f, 0.50f, 1.00f};
  ImVec4 border    {0.25f, 0.25f, 0.28f, 0.50f};

  c[ImGuiCol_Text]                  = text;
  c[ImGuiCol_TextDisabled]          = text_dim;
  c[ImGuiCol_WindowBg]              = bg;
  c[ImGuiCol_ChildBg]               = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_PopupBg]               = {0.14f, 0.14f, 0.16f, 0.96f};
  c[ImGuiCol_Border]                = border;
  c[ImGuiCol_BorderShadow]          = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_FrameBg]               = bg_light;
  c[ImGuiCol_FrameBgHovered]        = {0.20f, 0.20f, 0.22f, 1.00f};
  c[ImGuiCol_FrameBgActive]         = {0.24f, 0.24f, 0.26f, 1.00f};
  c[ImGuiCol_TitleBg]               = bg_dark;
  c[ImGuiCol_TitleBgActive]         = {0.13f, 0.13f, 0.15f, 1.00f};
  c[ImGuiCol_TitleBgCollapsed]      = {0.08f, 0.08f, 0.10f, 0.50f};
  c[ImGuiCol_MenuBarBg]             = {0.13f, 0.13f, 0.15f, 1.00f};
  c[ImGuiCol_ScrollbarBg]           = {0.08f, 0.08f, 0.10f, 0.50f};
  c[ImGuiCol_ScrollbarGrab]         = {0.30f, 0.30f, 0.33f, 1.00f};
  c[ImGuiCol_ScrollbarGrabHovered]  = {0.40f, 0.40f, 0.43f, 1.00f};
  c[ImGuiCol_ScrollbarGrabActive]   = {0.50f, 0.50f, 0.53f, 1.00f};
  c[ImGuiCol_CheckMark]             = accent_hi;
  c[ImGuiCol_SliderGrab]            = accent;
  c[ImGuiCol_SliderGrabActive]      = accent_hi;
  c[ImGuiCol_Button]                = {0.20f, 0.22f, 0.25f, 1.00f};
  c[ImGuiCol_ButtonHovered]         = accent_dim;
  c[ImGuiCol_ButtonActive]          = accent;
  c[ImGuiCol_Header]                = {0.18f, 0.18f, 0.20f, 1.00f};
  c[ImGuiCol_HeaderHovered]         = accent_dim;
  c[ImGuiCol_HeaderActive]          = accent;
  c[ImGuiCol_Separator]             = border;
  c[ImGuiCol_SeparatorHovered]      = accent_dim;
  c[ImGuiCol_SeparatorActive]       = accent;
  c[ImGuiCol_ResizeGrip]            = {0.20f, 0.20f, 0.22f, 0.50f};
  c[ImGuiCol_ResizeGripHovered]     = accent_dim;
  c[ImGuiCol_ResizeGripActive]      = accent;
  c[ImGuiCol_Tab]                   = {0.14f, 0.14f, 0.16f, 1.00f};
  c[ImGuiCol_TabHovered]            = {accent.x, accent.y, accent.z, 0.80f};
  c[ImGuiCol_TabSelected]           = accent_dim;
  c[ImGuiCol_TabSelectedOverline]   = accent;
  c[ImGuiCol_TabDimmed]             = {0.10f, 0.10f, 0.12f, 1.00f};
  c[ImGuiCol_TabDimmedSelected]     = {0.14f, 0.14f, 0.16f, 1.00f};
  c[ImGuiCol_DockingPreview]        = {accent.x, accent.y, accent.z, 0.70f};
  c[ImGuiCol_DockingEmptyBg]        = bg_dark;
  c[ImGuiCol_PlotLines]             = accent;
  c[ImGuiCol_PlotLinesHovered]      = accent_hi;
  c[ImGuiCol_PlotHistogram]         = accent;
  c[ImGuiCol_PlotHistogramHovered]  = accent_hi;
  c[ImGuiCol_TableHeaderBg]         = {0.14f, 0.14f, 0.16f, 1.00f};
  c[ImGuiCol_TableBorderStrong]     = {0.22f, 0.22f, 0.24f, 1.00f};
  c[ImGuiCol_TableBorderLight]      = {0.18f, 0.18f, 0.20f, 1.00f};
  c[ImGuiCol_TableRowBg]            = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_TableRowBgAlt]         = {0.06f, 0.06f, 0.06f, 0.40f};
  c[ImGuiCol_TextSelectedBg]        = {accent.x, accent.y, accent.z, 0.35f};
  c[ImGuiCol_DragDropTarget]        = accent_hi;
  c[ImGuiCol_NavHighlight]          = accent;
  c[ImGuiCol_NavWindowingHighlight] = {1.00f, 1.00f, 1.00f, 0.70f};
  c[ImGuiCol_NavWindowingDimBg]     = {0.80f, 0.80f, 0.80f, 0.20f};
  c[ImGuiCol_ModalWindowDimBg]      = {0.00f, 0.00f, 0.00f, 0.55f};

  s.WindowRounding    = 2.0f;
  s.FrameRounding     = 2.0f;
  s.GrabRounding      = 2.0f;
  s.TabRounding       = 2.0f;
  s.ScrollbarRounding = 2.0f;
  s.PopupRounding     = 2.0f;
  s.ChildRounding     = 2.0f;
  s.FrameBorderSize   = 0.0f;
  s.WindowBorderSize  = 1.0f;
  s.TabBorderSize     = 0.0f;
  s.TabBarBorderSize  = 1.0f;
  s.TabBarOverlineSize = 0.0f;
  s.SeparatorTextBorderSize = 1.0f;
  s.WindowMenuButtonPosition = ImGuiDir_None;
  s.WindowPadding     = {8.0f, 6.0f};
  s.FramePadding      = {6.0f, 3.0f};
  s.CellPadding       = {4.0f, 2.0f};
  s.ItemSpacing       = {8.0f, 4.0f};
  s.ItemInnerSpacing  = {4.0f, 4.0f};
  s.IndentSpacing     = 16.0f;
  s.ScrollbarSize     = 11.0f;
  s.GrabMinSize       = 8.0f;
  s.AntiAliasedLines  = true;
  s.AntiAliasedFill   = true;
  s.HoverDelayShort   = 0.15f;
  s.HoverDelayNormal  = 0.30f;
  s.HoverStationaryDelay = 0.15f;

  return {
      .status_connected    = {0.30f, 1.00f, 0.40f, 1.00f},
      .status_disconnected = {1.00f, 0.40f, 0.40f, 1.00f},
      .status_recording    = {1.00f, 0.30f, 0.30f, 1.00f},
      .byte_changed        = {1.00f, 0.80f, 0.00f, 1.00f},
      .new_frame_row_bg    = {0.20f, 0.40f, 0.10f, 0.40f},
      .error_text          = {1.00f, 0.40f, 0.40f, 1.00f},
      .active_source_label = {0.40f, 0.80f, 1.00f, 1.00f},
      .load_ok             = {0.20f, 0.80f, 0.20f, 1.00f},
      .load_warn           = {0.90f, 0.80f, 0.10f, 1.00f},
      .load_critical       = {1.00f, 0.30f, 0.20f, 1.00f},
      .live_button         = {0.15f, 0.55f, 0.15f, 1.00f},
      .paused_button       = {0.55f, 0.45f, 0.10f, 1.00f},
      .active_chart_header = {accent_dim.x, accent_dim.y, accent_dim.z, 0.60f},
      .chart_bg            = IM_COL32(20, 20, 25, 255),
      .chart_border        = IM_COL32(60, 60, 70, 255),
      .chart_grid          = IM_COL32(40, 40, 50, 255),
      .chart_grid_text     = IM_COL32(130, 130, 150, 255),
      .chart_cursor        = IM_COL32(200, 200, 200, 120),
      .editor_bg           = IM_COL32(25, 25, 35, 255),
      .editor_grid         = IM_COL32(55, 55, 75, 255),
      .editor_axis         = IM_COL32(120, 120, 140, 255),
      .editor_line         = IM_COL32(80, 180, 255, 255),
      .editor_point        = IM_COL32(255, 200, 60, 255),
      .editor_point_hl     = IM_COL32(255, 255, 130, 255),
      .editor_text         = IM_COL32(180, 180, 200, 255),
      .channel_on_chart    = {0.40f, 1.00f, 0.40f, 1.00f},
      .clear_color         = {0.08f, 0.08f, 0.10f, 1.00f},
  };
}

inline semantic_colors apply_midnight() {
  auto& s = ImGui::GetStyle();
  auto* c = s.Colors;

  ImVec4 bg        {0.07f, 0.07f, 0.12f, 1.00f};
  ImVec4 bg_dark   {0.05f, 0.05f, 0.09f, 1.00f};
  ImVec4 bg_light  {0.12f, 0.12f, 0.20f, 1.00f};
  ImVec4 accent    {0.45f, 0.35f, 0.75f, 1.00f};
  ImVec4 accent_dim{0.32f, 0.24f, 0.56f, 1.00f};
  ImVec4 accent_hi {0.55f, 0.45f, 0.85f, 1.00f};
  ImVec4 text      {0.90f, 0.90f, 0.95f, 1.00f};
  ImVec4 text_dim  {0.48f, 0.48f, 0.56f, 1.00f};
  ImVec4 border    {0.20f, 0.20f, 0.35f, 0.60f};

  c[ImGuiCol_Text]                  = text;
  c[ImGuiCol_TextDisabled]          = text_dim;
  c[ImGuiCol_WindowBg]              = bg;
  c[ImGuiCol_ChildBg]               = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_PopupBg]               = {0.10f, 0.10f, 0.16f, 0.96f};
  c[ImGuiCol_Border]                = border;
  c[ImGuiCol_BorderShadow]          = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_FrameBg]               = bg_light;
  c[ImGuiCol_FrameBgHovered]        = {0.16f, 0.16f, 0.26f, 1.00f};
  c[ImGuiCol_FrameBgActive]         = {0.20f, 0.20f, 0.30f, 1.00f};
  c[ImGuiCol_TitleBg]               = bg_dark;
  c[ImGuiCol_TitleBgActive]         = {0.10f, 0.10f, 0.16f, 1.00f};
  c[ImGuiCol_TitleBgCollapsed]      = {0.05f, 0.05f, 0.09f, 0.50f};
  c[ImGuiCol_MenuBarBg]             = {0.09f, 0.09f, 0.14f, 1.00f};
  c[ImGuiCol_ScrollbarBg]           = {0.06f, 0.06f, 0.10f, 0.50f};
  c[ImGuiCol_ScrollbarGrab]         = {0.28f, 0.28f, 0.38f, 1.00f};
  c[ImGuiCol_ScrollbarGrabHovered]  = {0.38f, 0.38f, 0.50f, 1.00f};
  c[ImGuiCol_ScrollbarGrabActive]   = {0.48f, 0.48f, 0.60f, 1.00f};
  c[ImGuiCol_CheckMark]             = accent_hi;
  c[ImGuiCol_SliderGrab]            = accent;
  c[ImGuiCol_SliderGrabActive]      = accent_hi;
  c[ImGuiCol_Button]                = {0.16f, 0.16f, 0.26f, 1.00f};
  c[ImGuiCol_ButtonHovered]         = accent_dim;
  c[ImGuiCol_ButtonActive]          = accent;
  c[ImGuiCol_Header]                = {0.14f, 0.14f, 0.22f, 1.00f};
  c[ImGuiCol_HeaderHovered]         = accent_dim;
  c[ImGuiCol_HeaderActive]          = accent;
  c[ImGuiCol_Separator]             = border;
  c[ImGuiCol_SeparatorHovered]      = accent_dim;
  c[ImGuiCol_SeparatorActive]       = accent;
  c[ImGuiCol_ResizeGrip]            = {0.18f, 0.18f, 0.28f, 0.50f};
  c[ImGuiCol_ResizeGripHovered]     = accent_dim;
  c[ImGuiCol_ResizeGripActive]      = accent;
  c[ImGuiCol_Tab]                   = {0.10f, 0.10f, 0.16f, 1.00f};
  c[ImGuiCol_TabHovered]            = {accent.x, accent.y, accent.z, 0.80f};
  c[ImGuiCol_TabSelected]           = accent_dim;
  c[ImGuiCol_TabSelectedOverline]   = accent;
  c[ImGuiCol_TabDimmed]             = {0.08f, 0.08f, 0.12f, 1.00f};
  c[ImGuiCol_TabDimmedSelected]     = {0.12f, 0.12f, 0.18f, 1.00f};
  c[ImGuiCol_DockingPreview]        = {accent.x, accent.y, accent.z, 0.70f};
  c[ImGuiCol_DockingEmptyBg]        = bg_dark;
  c[ImGuiCol_PlotLines]             = accent;
  c[ImGuiCol_PlotLinesHovered]      = accent_hi;
  c[ImGuiCol_PlotHistogram]         = accent;
  c[ImGuiCol_PlotHistogramHovered]  = accent_hi;
  c[ImGuiCol_TableHeaderBg]         = {0.10f, 0.10f, 0.16f, 1.00f};
  c[ImGuiCol_TableBorderStrong]     = {0.18f, 0.18f, 0.28f, 1.00f};
  c[ImGuiCol_TableBorderLight]      = {0.14f, 0.14f, 0.22f, 1.00f};
  c[ImGuiCol_TableRowBg]            = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_TableRowBgAlt]         = {0.04f, 0.04f, 0.08f, 0.40f};
  c[ImGuiCol_TextSelectedBg]        = {accent.x, accent.y, accent.z, 0.35f};
  c[ImGuiCol_DragDropTarget]        = accent_hi;
  c[ImGuiCol_NavHighlight]          = accent;
  c[ImGuiCol_NavWindowingHighlight] = {1.00f, 1.00f, 1.00f, 0.70f};
  c[ImGuiCol_NavWindowingDimBg]     = {0.80f, 0.80f, 0.80f, 0.20f};
  c[ImGuiCol_ModalWindowDimBg]      = {0.00f, 0.00f, 0.00f, 0.55f};

  s.WindowRounding    = 2.0f;
  s.FrameRounding     = 2.0f;
  s.GrabRounding      = 2.0f;
  s.TabRounding       = 2.0f;
  s.ScrollbarRounding = 2.0f;
  s.PopupRounding     = 2.0f;
  s.ChildRounding     = 2.0f;
  s.FrameBorderSize   = 0.0f;
  s.WindowBorderSize  = 1.0f;
  s.TabBorderSize     = 0.0f;
  s.TabBarBorderSize  = 1.0f;
  s.TabBarOverlineSize = 0.0f;
  s.SeparatorTextBorderSize = 1.0f;
  s.WindowMenuButtonPosition = ImGuiDir_None;
  s.WindowPadding     = {8.0f, 6.0f};
  s.FramePadding      = {6.0f, 3.0f};
  s.CellPadding       = {4.0f, 2.0f};
  s.ItemSpacing       = {8.0f, 4.0f};
  s.ItemInnerSpacing  = {4.0f, 4.0f};
  s.IndentSpacing     = 16.0f;
  s.ScrollbarSize     = 11.0f;
  s.GrabMinSize       = 8.0f;
  s.AntiAliasedLines  = true;
  s.AntiAliasedFill   = true;
  s.HoverDelayShort   = 0.15f;
  s.HoverDelayNormal  = 0.30f;
  s.HoverStationaryDelay = 0.15f;

  return {
      .status_connected    = {0.40f, 1.00f, 0.50f, 1.00f},
      .status_disconnected = {1.00f, 0.40f, 0.50f, 1.00f},
      .status_recording    = {1.00f, 0.35f, 0.35f, 1.00f},
      .byte_changed        = {1.00f, 0.75f, 0.20f, 1.00f},
      .new_frame_row_bg    = {0.15f, 0.25f, 0.40f, 0.40f},
      .error_text          = {1.00f, 0.45f, 0.50f, 1.00f},
      .active_source_label = {0.55f, 0.65f, 1.00f, 1.00f},
      .load_ok             = {0.30f, 0.80f, 0.40f, 1.00f},
      .load_warn           = {0.90f, 0.75f, 0.20f, 1.00f},
      .load_critical       = {1.00f, 0.35f, 0.30f, 1.00f},
      .live_button         = {0.20f, 0.50f, 0.25f, 1.00f},
      .paused_button       = {0.50f, 0.40f, 0.15f, 1.00f},
      .active_chart_header = {accent_dim.x, accent_dim.y, accent_dim.z, 0.60f},
      .chart_bg            = IM_COL32(14, 14, 24, 255),
      .chart_border        = IM_COL32(50, 50, 80, 255),
      .chart_grid          = IM_COL32(35, 35, 55, 255),
      .chart_grid_text     = IM_COL32(110, 110, 150, 255),
      .chart_cursor        = IM_COL32(180, 180, 220, 120),
      .editor_bg           = IM_COL32(18, 18, 30, 255),
      .editor_grid         = IM_COL32(40, 40, 65, 255),
      .editor_axis         = IM_COL32(100, 100, 140, 255),
      .editor_line         = IM_COL32(110, 80, 220, 255),
      .editor_point        = IM_COL32(230, 180, 80, 255),
      .editor_point_hl     = IM_COL32(255, 230, 140, 255),
      .editor_text         = IM_COL32(160, 160, 200, 255),
      .channel_on_chart    = {0.45f, 1.00f, 0.45f, 1.00f},
      .clear_color         = {0.05f, 0.05f, 0.09f, 1.00f},
  };
}

inline semantic_colors apply_automotive() {
  auto& s = ImGui::GetStyle();
  auto* c = s.Colors;

  ImVec4 bg        {0.15f, 0.15f, 0.16f, 1.00f};
  ImVec4 bg_dark   {0.11f, 0.11f, 0.12f, 1.00f};
  ImVec4 bg_light  {0.20f, 0.20f, 0.21f, 1.00f};
  ImVec4 accent    {0.26f, 0.46f, 0.62f, 1.00f};
  ImVec4 accent_dim{0.20f, 0.36f, 0.50f, 1.00f};
  ImVec4 accent_hi {0.32f, 0.56f, 0.72f, 1.00f};
  ImVec4 text      {0.88f, 0.88f, 0.86f, 1.00f};
  ImVec4 text_dim  {0.52f, 0.52f, 0.50f, 1.00f};
  ImVec4 border    {0.30f, 0.30f, 0.32f, 0.80f};

  c[ImGuiCol_Text]                  = text;
  c[ImGuiCol_TextDisabled]          = text_dim;
  c[ImGuiCol_WindowBg]              = bg;
  c[ImGuiCol_ChildBg]               = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_PopupBg]               = {0.17f, 0.17f, 0.18f, 0.96f};
  c[ImGuiCol_Border]                = border;
  c[ImGuiCol_BorderShadow]          = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_FrameBg]               = bg_light;
  c[ImGuiCol_FrameBgHovered]        = {0.24f, 0.24f, 0.25f, 1.00f};
  c[ImGuiCol_FrameBgActive]         = {0.28f, 0.28f, 0.29f, 1.00f};
  c[ImGuiCol_TitleBg]               = bg_dark;
  c[ImGuiCol_TitleBgActive]         = {0.14f, 0.14f, 0.15f, 1.00f};
  c[ImGuiCol_TitleBgCollapsed]      = {0.11f, 0.11f, 0.12f, 0.50f};
  c[ImGuiCol_MenuBarBg]             = {0.14f, 0.14f, 0.15f, 1.00f};
  c[ImGuiCol_ScrollbarBg]           = {0.12f, 0.12f, 0.13f, 0.50f};
  c[ImGuiCol_ScrollbarGrab]         = {0.32f, 0.32f, 0.34f, 1.00f};
  c[ImGuiCol_ScrollbarGrabHovered]  = {0.42f, 0.42f, 0.44f, 1.00f};
  c[ImGuiCol_ScrollbarGrabActive]   = {0.52f, 0.52f, 0.54f, 1.00f};
  c[ImGuiCol_CheckMark]             = accent_hi;
  c[ImGuiCol_SliderGrab]            = accent;
  c[ImGuiCol_SliderGrabActive]      = accent_hi;
  c[ImGuiCol_Button]                = {0.22f, 0.22f, 0.24f, 1.00f};
  c[ImGuiCol_ButtonHovered]         = accent_dim;
  c[ImGuiCol_ButtonActive]          = accent;
  c[ImGuiCol_Header]                = {0.18f, 0.18f, 0.20f, 1.00f};
  c[ImGuiCol_HeaderHovered]         = accent_dim;
  c[ImGuiCol_HeaderActive]          = accent;
  c[ImGuiCol_Separator]             = border;
  c[ImGuiCol_SeparatorHovered]      = accent_dim;
  c[ImGuiCol_SeparatorActive]       = accent;
  c[ImGuiCol_ResizeGrip]            = {0.22f, 0.22f, 0.24f, 0.50f};
  c[ImGuiCol_ResizeGripHovered]     = accent_dim;
  c[ImGuiCol_ResizeGripActive]      = accent;
  c[ImGuiCol_Tab]                   = {0.13f, 0.13f, 0.14f, 1.00f};
  c[ImGuiCol_TabHovered]            = {accent.x, accent.y, accent.z, 0.80f};
  c[ImGuiCol_TabSelected]           = accent_dim;
  c[ImGuiCol_TabSelectedOverline]   = accent;
  c[ImGuiCol_TabDimmed]             = {0.11f, 0.11f, 0.12f, 1.00f};
  c[ImGuiCol_TabDimmedSelected]     = {0.14f, 0.14f, 0.15f, 1.00f};
  c[ImGuiCol_DockingPreview]        = {accent.x, accent.y, accent.z, 0.70f};
  c[ImGuiCol_DockingEmptyBg]        = bg_dark;
  c[ImGuiCol_PlotLines]             = accent;
  c[ImGuiCol_PlotLinesHovered]      = accent_hi;
  c[ImGuiCol_PlotHistogram]         = accent;
  c[ImGuiCol_PlotHistogramHovered]  = accent_hi;
  c[ImGuiCol_TableHeaderBg]         = {0.13f, 0.13f, 0.14f, 1.00f};
  c[ImGuiCol_TableBorderStrong]     = {0.26f, 0.26f, 0.28f, 1.00f};
  c[ImGuiCol_TableBorderLight]      = {0.22f, 0.22f, 0.24f, 1.00f};
  c[ImGuiCol_TableRowBg]            = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_TableRowBgAlt]         = {0.05f, 0.05f, 0.05f, 0.30f};
  c[ImGuiCol_TextSelectedBg]        = {accent.x, accent.y, accent.z, 0.35f};
  c[ImGuiCol_DragDropTarget]        = accent_hi;
  c[ImGuiCol_NavHighlight]          = accent;
  c[ImGuiCol_NavWindowingHighlight] = {1.00f, 1.00f, 1.00f, 0.70f};
  c[ImGuiCol_NavWindowingDimBg]     = {0.80f, 0.80f, 0.80f, 0.20f};
  c[ImGuiCol_ModalWindowDimBg]      = {0.00f, 0.00f, 0.00f, 0.55f};

  s.WindowRounding    = 2.0f;
  s.FrameRounding     = 2.0f;
  s.GrabRounding      = 2.0f;
  s.TabRounding       = 2.0f;
  s.ScrollbarRounding = 2.0f;
  s.PopupRounding     = 2.0f;
  s.ChildRounding     = 2.0f;
  s.FrameBorderSize   = 0.0f;
  s.WindowBorderSize  = 1.0f;
  s.TabBorderSize     = 0.0f;
  s.TabBarBorderSize  = 1.0f;
  s.TabBarOverlineSize = 0.0f;
  s.SeparatorTextBorderSize = 1.0f;
  s.WindowMenuButtonPosition = ImGuiDir_None;
  s.WindowPadding     = {8.0f, 6.0f};
  s.FramePadding      = {6.0f, 3.0f};
  s.CellPadding       = {4.0f, 2.0f};
  s.ItemSpacing       = {8.0f, 4.0f};
  s.ItemInnerSpacing  = {4.0f, 4.0f};
  s.IndentSpacing     = 16.0f;
  s.ScrollbarSize     = 11.0f;
  s.GrabMinSize       = 8.0f;
  s.AntiAliasedLines  = true;
  s.AntiAliasedFill   = true;
  s.HoverDelayShort   = 0.15f;
  s.HoverDelayNormal  = 0.30f;
  s.HoverStationaryDelay = 0.15f;

  return {
      .status_connected    = {0.30f, 0.90f, 0.35f, 1.00f},
      .status_disconnected = {0.90f, 0.35f, 0.35f, 1.00f},
      .status_recording    = {0.95f, 0.25f, 0.25f, 1.00f},
      .byte_changed        = {1.00f, 0.78f, 0.10f, 1.00f},
      .new_frame_row_bg    = {0.18f, 0.35f, 0.12f, 0.40f},
      .error_text          = {0.95f, 0.35f, 0.35f, 1.00f},
      .active_source_label = {0.40f, 0.70f, 0.95f, 1.00f},
      .load_ok             = {0.25f, 0.75f, 0.25f, 1.00f},
      .load_warn           = {0.85f, 0.75f, 0.15f, 1.00f},
      .load_critical       = {0.95f, 0.30f, 0.25f, 1.00f},
      .live_button         = {0.18f, 0.50f, 0.18f, 1.00f},
      .paused_button       = {0.50f, 0.42f, 0.12f, 1.00f},
      .active_chart_header = {accent_dim.x, accent_dim.y, accent_dim.z, 0.60f},
      .chart_bg            = IM_COL32(26, 26, 28, 255),
      .chart_border        = IM_COL32(70, 70, 74, 255),
      .chart_grid          = IM_COL32(46, 46, 50, 255),
      .chart_grid_text     = IM_COL32(140, 140, 144, 255),
      .chart_cursor        = IM_COL32(200, 200, 200, 120),
      .editor_bg           = IM_COL32(30, 30, 33, 255),
      .editor_grid         = IM_COL32(58, 58, 62, 255),
      .editor_axis         = IM_COL32(125, 125, 130, 255),
      .editor_line         = IM_COL32(70, 140, 200, 255),
      .editor_point        = IM_COL32(240, 190, 60, 255),
      .editor_point_hl     = IM_COL32(255, 240, 120, 255),
      .editor_text         = IM_COL32(170, 170, 175, 255),
      .channel_on_chart    = {0.35f, 0.95f, 0.35f, 1.00f},
      .clear_color         = {0.10f, 0.10f, 0.11f, 1.00f},
  };
}

inline semantic_colors apply_light() {
  auto& s = ImGui::GetStyle();
  auto* c = s.Colors;

  ImVec4 bg        {0.95f, 0.95f, 0.96f, 1.00f};
  ImVec4 bg_dark   {0.88f, 0.88f, 0.90f, 1.00f};
  ImVec4 bg_light  {0.90f, 0.90f, 0.92f, 1.00f};
  ImVec4 accent    {0.20f, 0.42f, 0.68f, 1.00f};
  ImVec4 accent_dim{0.28f, 0.50f, 0.74f, 0.70f};
  ImVec4 accent_hi {0.24f, 0.50f, 0.78f, 1.00f};
  ImVec4 text      {0.10f, 0.10f, 0.10f, 1.00f};
  ImVec4 text_dim  {0.50f, 0.50f, 0.50f, 1.00f};
  ImVec4 border    {0.70f, 0.70f, 0.72f, 0.60f};

  c[ImGuiCol_Text]                  = text;
  c[ImGuiCol_TextDisabled]          = text_dim;
  c[ImGuiCol_WindowBg]              = bg;
  c[ImGuiCol_ChildBg]               = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_PopupBg]               = {0.98f, 0.98f, 0.99f, 0.98f};
  c[ImGuiCol_Border]                = border;
  c[ImGuiCol_BorderShadow]          = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_FrameBg]               = bg_light;
  c[ImGuiCol_FrameBgHovered]        = {0.85f, 0.85f, 0.88f, 1.00f};
  c[ImGuiCol_FrameBgActive]         = {0.80f, 0.80f, 0.84f, 1.00f};
  c[ImGuiCol_TitleBg]               = bg_dark;
  c[ImGuiCol_TitleBgActive]         = {0.84f, 0.84f, 0.87f, 1.00f};
  c[ImGuiCol_TitleBgCollapsed]      = {0.92f, 0.92f, 0.94f, 0.50f};
  c[ImGuiCol_MenuBarBg]             = {0.92f, 0.92f, 0.94f, 1.00f};
  c[ImGuiCol_ScrollbarBg]           = {0.90f, 0.90f, 0.92f, 0.50f};
  c[ImGuiCol_ScrollbarGrab]         = {0.72f, 0.72f, 0.74f, 1.00f};
  c[ImGuiCol_ScrollbarGrabHovered]  = {0.62f, 0.62f, 0.64f, 1.00f};
  c[ImGuiCol_ScrollbarGrabActive]   = {0.52f, 0.52f, 0.54f, 1.00f};
  c[ImGuiCol_CheckMark]             = accent;
  c[ImGuiCol_SliderGrab]            = accent;
  c[ImGuiCol_SliderGrabActive]      = accent_hi;
  c[ImGuiCol_Button]                = {0.85f, 0.85f, 0.88f, 1.00f};
  c[ImGuiCol_ButtonHovered]         = accent_dim;
  c[ImGuiCol_ButtonActive]          = accent;
  c[ImGuiCol_Header]                = {0.88f, 0.88f, 0.90f, 1.00f};
  c[ImGuiCol_HeaderHovered]         = accent_dim;
  c[ImGuiCol_HeaderActive]          = accent;
  c[ImGuiCol_Separator]             = border;
  c[ImGuiCol_SeparatorHovered]      = accent_dim;
  c[ImGuiCol_SeparatorActive]       = accent;
  c[ImGuiCol_ResizeGrip]            = {0.78f, 0.78f, 0.80f, 0.50f};
  c[ImGuiCol_ResizeGripHovered]     = accent_dim;
  c[ImGuiCol_ResizeGripActive]      = accent;
  c[ImGuiCol_Tab]                   = {0.90f, 0.90f, 0.92f, 1.00f};
  c[ImGuiCol_TabHovered]            = {accent.x, accent.y, accent.z, 0.50f};
  c[ImGuiCol_TabSelected]           = {0.96f, 0.96f, 0.98f, 1.00f};
  c[ImGuiCol_TabSelectedOverline]   = accent;
  c[ImGuiCol_TabDimmed]             = {0.92f, 0.92f, 0.94f, 1.00f};
  c[ImGuiCol_TabDimmedSelected]     = {0.94f, 0.94f, 0.96f, 1.00f};
  c[ImGuiCol_DockingPreview]        = {accent.x, accent.y, accent.z, 0.50f};
  c[ImGuiCol_DockingEmptyBg]        = bg;
  c[ImGuiCol_PlotLines]             = accent;
  c[ImGuiCol_PlotLinesHovered]      = accent_hi;
  c[ImGuiCol_PlotHistogram]         = accent;
  c[ImGuiCol_PlotHistogramHovered]  = accent_hi;
  c[ImGuiCol_TableHeaderBg]         = {0.88f, 0.88f, 0.90f, 1.00f};
  c[ImGuiCol_TableBorderStrong]     = {0.76f, 0.76f, 0.78f, 1.00f};
  c[ImGuiCol_TableBorderLight]      = {0.82f, 0.82f, 0.84f, 1.00f};
  c[ImGuiCol_TableRowBg]            = {0.00f, 0.00f, 0.00f, 0.00f};
  c[ImGuiCol_TableRowBgAlt]         = {0.00f, 0.00f, 0.00f, 0.04f};
  c[ImGuiCol_TextSelectedBg]        = {accent.x, accent.y, accent.z, 0.25f};
  c[ImGuiCol_DragDropTarget]        = accent_hi;
  c[ImGuiCol_NavHighlight]          = accent;
  c[ImGuiCol_NavWindowingHighlight] = {1.00f, 1.00f, 1.00f, 0.70f};
  c[ImGuiCol_NavWindowingDimBg]     = {0.20f, 0.20f, 0.20f, 0.20f};
  c[ImGuiCol_ModalWindowDimBg]      = {0.00f, 0.00f, 0.00f, 0.35f};

  s.WindowRounding    = 2.0f;
  s.FrameRounding     = 2.0f;
  s.GrabRounding      = 2.0f;
  s.TabRounding       = 2.0f;
  s.ScrollbarRounding = 2.0f;
  s.PopupRounding     = 2.0f;
  s.ChildRounding     = 2.0f;
  s.FrameBorderSize   = 0.0f;
  s.WindowBorderSize  = 1.0f;
  s.TabBorderSize     = 0.0f;
  s.TabBarBorderSize  = 1.0f;
  s.TabBarOverlineSize = 0.0f;
  s.SeparatorTextBorderSize = 1.0f;
  s.WindowMenuButtonPosition = ImGuiDir_None;
  s.WindowPadding     = {8.0f, 6.0f};
  s.FramePadding      = {6.0f, 3.0f};
  s.CellPadding       = {4.0f, 2.0f};
  s.ItemSpacing       = {8.0f, 4.0f};
  s.ItemInnerSpacing  = {4.0f, 4.0f};
  s.IndentSpacing     = 16.0f;
  s.ScrollbarSize     = 11.0f;
  s.GrabMinSize       = 8.0f;
  s.AntiAliasedLines  = true;
  s.AntiAliasedFill   = true;
  s.HoverDelayShort   = 0.15f;
  s.HoverDelayNormal  = 0.30f;
  s.HoverStationaryDelay = 0.15f;

  return {
      .status_connected    = {0.10f, 0.65f, 0.20f, 1.00f},
      .status_disconnected = {0.80f, 0.20f, 0.20f, 1.00f},
      .status_recording    = {0.85f, 0.15f, 0.15f, 1.00f},
      .byte_changed        = {0.80f, 0.55f, 0.00f, 1.00f},
      .new_frame_row_bg    = {0.70f, 0.90f, 0.70f, 0.30f},
      .error_text          = {0.85f, 0.20f, 0.20f, 1.00f},
      .active_source_label = {0.15f, 0.45f, 0.80f, 1.00f},
      .load_ok             = {0.15f, 0.60f, 0.15f, 1.00f},
      .load_warn           = {0.75f, 0.60f, 0.05f, 1.00f},
      .load_critical       = {0.80f, 0.20f, 0.15f, 1.00f},
      .live_button         = {0.15f, 0.55f, 0.15f, 1.00f},
      .paused_button       = {0.60f, 0.50f, 0.10f, 1.00f},
      .active_chart_header = {accent.x, accent.y, accent.z, 0.20f},
      .chart_bg            = IM_COL32(252, 252, 254, 255),
      .chart_border        = IM_COL32(190, 190, 195, 255),
      .chart_grid          = IM_COL32(220, 220, 225, 255),
      .chart_grid_text     = IM_COL32(100, 100, 110, 255),
      .chart_cursor        = IM_COL32(80, 80, 90, 120),
      .editor_bg           = IM_COL32(248, 248, 252, 255),
      .editor_grid         = IM_COL32(215, 215, 225, 255),
      .editor_axis         = IM_COL32(120, 120, 130, 255),
      .editor_line         = IM_COL32(50, 110, 180, 255),
      .editor_point        = IM_COL32(220, 150, 30, 255),
      .editor_point_hl     = IM_COL32(240, 190, 60, 255),
      .editor_text         = IM_COL32(80, 80, 90, 255),
      .channel_on_chart    = {0.10f, 0.60f, 0.10f, 1.00f},
      .clear_color         = {0.92f, 0.92f, 0.94f, 1.00f},
  };
}

}  // namespace detail

[[nodiscard]] inline semantic_colors apply_theme(theme_id id, float ui_scale) {
  ImGui::GetStyle() = ImGuiStyle();

  semantic_colors colors;
  switch (id) {
    case theme_id::dark_flat:  colors = detail::apply_dark_flat();  break;
    case theme_id::midnight:   colors = detail::apply_midnight();   break;
    case theme_id::automotive: colors = detail::apply_automotive(); break;
    case theme_id::light:      colors = detail::apply_light();      break;
    default:                   colors = detail::apply_dark_flat();  break;
  }

  ImGui::GetStyle().ScaleAllSizes(ui_scale);
  return colors;
}

inline bool checkbox(const char* label, bool* v) {
  ImGuiWindow* window = ImGui::GetCurrentWindow();
  if (window->SkipItems) return false;

  const ImGuiStyle& style = ImGui::GetStyle();
  const ImGuiID id = window->GetID(label);
  const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

  const float square_sz = ImGui::GetFrameHeight();
  const ImVec2 pos = window->DC.CursorPos;
  const ImRect total_bb(
      pos,
      ImVec2(pos.x + square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f),
             pos.y + std::max(square_sz, label_size.y + style.FramePadding.y * 2.0f)));
  ImGui::ItemSize(total_bb, style.FramePadding.y);
  if (!ImGui::ItemAdd(total_bb, id)) return false;

  bool hovered, held;
  bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
  if (pressed) *v = !(*v);

  const ImRect check_bb(pos, ImVec2(pos.x + square_sz, pos.y + square_sz));
  ImU32 bg_col;
  if (*v)
    bg_col = ImGui::GetColorU32(hovered ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);
  else
    bg_col = ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
  float rounding = style.FrameRounding;
  ImGui::RenderFrame(check_bb.Min, check_bb.Max, bg_col, false, rounding);

  if (*v) {
    float pad = std::max(1.0f, square_sz * 0.15f);
    ImU32 fill = ImGui::GetColorU32(ImGuiCol_CheckMark);
    window->DrawList->AddRectFilled(
        ImVec2(check_bb.Min.x + pad, check_bb.Min.y + pad),
        ImVec2(check_bb.Max.x - pad, check_bb.Max.y - pad),
        fill, rounding);
  }

  ImVec2 label_pos = ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x,
                             check_bb.Min.y + style.FramePadding.y);
  if (label_size.x > 0.0f)
    ImGui::RenderText(label_pos, label);

  return pressed;
}

}  // namespace jcan
