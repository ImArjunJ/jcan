#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <nfd.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include "app_state.hpp"
#include "async_dialog.hpp"
#include "discovery.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "theme.hpp"
#include "widgets/connection.hpp"
#include "widgets/monitor.hpp"
#include "widgets/plotter.hpp"
#include "widgets/signals.hpp"
#include "widgets/statistics.hpp"
#include "widgets/transmitter.hpp"

static void glfw_error_callback(int error, const char* description) {
  std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

enum class dialog_id { none, open_dbc, open_replay, import_log, export_log };

static jcan::app_state* g_drop_state = nullptr;

static void glfw_drop_callback([[maybe_unused]] GLFWwindow* window, int count,
                               const char** paths) {
  if (!g_drop_state || count <= 0) return;
  if (!g_drop_state->log_mode) return;
  for (int i = 0; i < count; ++i) {
    std::string path(paths[i]);
    if (path.size() >= 4) {
      std::string ext = path.substr(path.size() - 4);
      for (auto& c : ext) c = static_cast<char>(std::tolower(c));
      if (ext == ".dbc") {
        auto err = g_drop_state->dbc.load(path);
        if (err.empty()) {
          g_drop_state->redecode_log();
          g_drop_state->status_text = std::format(
              "DBC: {} msgs", g_drop_state->dbc.message_ids().size());
        } else {
          g_drop_state->status_text = err;
        }
        return;
      }
    }
  }
}

static void setup_default_layout(ImGuiID dockspace_id) {
  ImGui::DockBuilderRemoveNode(dockspace_id);
  ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

  ImGuiID top, bottom;
  ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.55f, &top, &bottom);

  ImGuiID bot_left, bot_right;
  ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 0.60f, &bot_left,
                              &bot_right);

  ImGui::DockBuilderDockWindow("Bus Monitor - Live", top);
  ImGui::DockBuilderDockWindow("Signals", top);
  ImGui::DockBuilderDockWindow("Analysis", top);
  ImGui::DockBuilderDockWindow("###scrollback", bot_left);
  ImGui::DockBuilderDockWindow("Bus Statistics", bot_left);
  ImGui::DockBuilderDockWindow("Transmitter", bot_right);

  ImGui::DockBuilderFinish(dockspace_id);
}

int main() {
  try {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    NFD_Init();

    jcan::async_dialog file_dialog;
    dialog_id pending_dialog = dialog_id::none;
    uint8_t pending_dbc_channel = 0xff;

    jcan::settings settings;
    settings.load();

    GLFWwindow* window =
        glfwCreateWindow(settings.window_width, settings.window_height,
                         "jcan - CAN Bus Tool", nullptr, nullptr);
    if (!window) {
      glfwTerminate();
      return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingWithShift = true;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    std::string mono_font_path;
    {
      const char* mono_paths[] = {
#ifdef _WIN32
          "C:\\Windows\\Fonts\\CascadiaMono.ttf",
          "C:\\Windows\\Fonts\\consola.ttf",
          "C:\\Windows\\Fonts\\cour.ttf",
#else
          "/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf",
          "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf",
          "/usr/share/fonts/TTF/FiraCode-Regular.ttf",
          "/usr/share/fonts/truetype/firacode/FiraCode-Regular.ttf",
          "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
          "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
#endif
          nullptr,
      };
      for (const char** p = mono_paths; *p; ++p) {
        if (std::filesystem::exists(*p)) {
          mono_font_path = *p;
          break;
        }
      }
    }

    float current_scale = settings.ui_scale;
    ImFont* mono_font = nullptr;
    {
      io.Fonts->Clear();
      float font_size = std::round(14.0f * current_scale);
      if (!mono_font_path.empty())
        mono_font =
            io.Fonts->AddFontFromFileTTF(mono_font_path.c_str(), font_size);
      io.Fonts->Build();
    }

    auto state_storage = std::make_unique<jcan::app_state>();
    auto& state = *state_storage;
    state.mono_font = mono_font;
    state.ui_scale = current_scale;
    state.current_theme = static_cast<jcan::theme_id>(settings.theme);
    state.colors = jcan::apply_theme(state.current_theme, current_scale);

    state.selected_bitrate = settings.selected_bitrate;
    state.show_signals = settings.show_signals;
    state.show_transmitter = settings.show_transmitter;
    state.show_statistics = settings.show_statistics;
    state.show_plotter = settings.show_plotter;
    state.log_dir = settings.effective_log_dir();

    for (const auto& p : settings.dbc_paths) {
      if (std::filesystem::exists(p)) state.dbc.load(p);
    }

    state.devices = jcan::discover_adapters();

    if (!settings.last_adapter_port.empty()) {
      for (int i = 0; i < static_cast<int>(state.devices.size()); ++i) {
        if (state.devices[i].port == settings.last_adapter_port) {
          state.selected_device = i;
          break;
        }
      }
    }

    g_drop_state = &state;
    glfwSetDropCallback(window, glfw_drop_callback);

    bool first_frame = true;
    bool was_focused = true;
    float pending_scale = 0.f;
    bool pending_theme = false;
    bool pending_import_confirm = false;
    jcan::widgets::plotter_state plotter;

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();

      bool focused = glfwGetWindowAttrib(window, GLFW_FOCUSED);
      bool iconified = glfwGetWindowAttrib(window, GLFW_ICONIFIED);

      if (focused != was_focused) {
        glfwSwapInterval(focused ? 1 : 0);
        was_focused = focused;
      }

      if (iconified) {
        state.poll_frames();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }

      if (pending_scale > 0.f || pending_theme) {
        float scale = (pending_scale > 0.f) ? pending_scale : current_scale;
        state.colors = jcan::apply_theme(state.current_theme, scale);
        io.Fonts->Clear();
        float font_size = std::round(14.0f * scale);
        if (!mono_font_path.empty())
          mono_font =
              io.Fonts->AddFontFromFileTTF(mono_font_path.c_str(), font_size);
        io.Fonts->Build();
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        ImGui_ImplOpenGL3_CreateFontsTexture();
        state.mono_font = mono_font;
        if (pending_scale > 0.f) current_scale = pending_scale;
        pending_scale = 0.f;
        pending_theme = false;
      }

      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(
          0, ImGui::GetMainViewport(),
          ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoUndocking);

      if (first_frame) {
        setup_default_layout(dockspace_id);
        first_frame = false;
      }

      if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
          if (state.log_mode && !state.imported_frames.empty()) {
            if (state.log_channels.size() <= 1) {
              if (ImGui::MenuItem("Load DBC...", "Ctrl+O", false,
                                  !file_dialog.busy())) {
                pending_dbc_channel = 0xff;
                file_dialog.open_file({{"DBC Files", "dbc"}});
                pending_dialog = dialog_id::open_dbc;
              }
              if (state.dbc.loaded()) {
                ImGui::SameLine();
                ImGui::TextDisabled("(%s)",
                                    state.dbc.filenames().front().c_str());
                if (ImGui::MenuItem("Unload DBC")) {
                  state.dbc.unload();
                  state.redecode_log();
                }
              }
            } else {
              if (ImGui::BeginMenu("Log DBC")) {
                for (uint8_t ch : state.log_channels) {
                  ImGui::PushID(ch);
                  auto it = state.log_dbc.find(ch);
                  bool has = it != state.log_dbc.end() && it->second.loaded();
                  auto label = std::format("Ch {}", static_cast<int>(ch));
                  if (has) {
                    ImGui::TextDisabled("%s: %s", label.c_str(),
                                        it->second.filenames().front().c_str());
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Unload")) {
                      state.log_dbc.erase(ch);
                      state.redecode_log();
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Change...")) {
                      pending_dbc_channel = ch;
                      file_dialog.open_file({{"DBC Files", "dbc"}});
                      pending_dialog = dialog_id::open_dbc;
                    }
                  } else {
                    auto btn = std::format("Load DBC for {}...", label);
                    if (ImGui::MenuItem(btn.c_str(), nullptr, false,
                                        !file_dialog.busy())) {
                      pending_dbc_channel = ch;
                      file_dialog.open_file({{"DBC Files", "dbc"}});
                      pending_dialog = dialog_id::open_dbc;
                    }
                  }
                  ImGui::PopID();
                }
                ImGui::EndMenu();
              }
            }
            ImGui::Separator();
          }
          if (state.logger.recording() && !state.exporting.load()) {
            auto log_label = std::format("Logging to: {} ({} frames)",
                                         state.logger.filename(),
                                         state.logger.frame_count());
            ImGui::MenuItem(log_label.c_str(), nullptr, false, false);
            if (ImGui::MenuItem("New Log", "Ctrl+R")) {
              state.logger.stop();
              state.auto_start_session_log();
            }
            if (ImGui::MenuItem("Export Log...", "Ctrl+E", false,
                                !file_dialog.busy())) {
              file_dialog.save_file({{"CSV Log", "csv"}, {"Vector ASC", "asc"}},
                                    "export.csv");
              pending_dialog = dialog_id::export_log;
            }
          }
          if (state.exporting.load()) {
            auto pct = state.export_progress.load() * 100.f;
            auto label = std::format("Exporting... {:.0f}%%", pct);
            ImGui::MenuItem(label.c_str(), nullptr, false, false);
          }
          if (ImGui::MenuItem("Import Log...", "Ctrl+I", false,
                              !file_dialog.busy())) {
            if (state.connected) {
              pending_import_confirm = true;
            } else {
              file_dialog.open_file({{"All Logs", "csv,asc,ld"},
                                     {"MoTec i2", "ld"},
                                     {"CSV / ASC", "csv,asc"}});
              pending_dialog = dialog_id::import_log;
            }
          }
          if (!state.replaying.load()) {
            if (ImGui::MenuItem("Replay Log...", nullptr, false,
                                !file_dialog.busy())) {
              file_dialog.open_file({{"CSV / ASC Log", "csv,asc"}});
              pending_dialog = dialog_id::open_replay;
            }
          } else {
            bool paused = state.replay_paused.load();
            if (ImGui::MenuItem(paused ? "Resume Replay" : "Pause Replay"))
              state.replay_paused.store(!paused);

            if (ImGui::BeginMenu("Replay Speed")) {
              float cur = state.replay_speed.load();
              if (ImGui::MenuItem("0.25x", nullptr, cur < 0.3f))
                state.replay_speed.store(0.25f);
              if (ImGui::MenuItem("0.5x", nullptr, cur > 0.4f && cur < 0.6f))
                state.replay_speed.store(0.5f);
              if (ImGui::MenuItem("1x", nullptr, cur > 0.9f && cur < 1.1f))
                state.replay_speed.store(1.0f);
              if (ImGui::MenuItem("2x", nullptr, cur > 1.9f && cur < 2.1f))
                state.replay_speed.store(2.0f);
              if (ImGui::MenuItem("4x", nullptr, cur > 3.9f && cur < 4.1f))
                state.replay_speed.store(4.0f);
              if (ImGui::MenuItem("10x", nullptr, cur > 9.9f))
                state.replay_speed.store(10.0f);
              ImGui::EndMenu();
            }

            auto replay_label = std::format(
                "Stop Replay ({:.0f}%)", state.replay_progress.load() * 100.f);
            if (ImGui::MenuItem(replay_label.c_str())) {
              state.stop_replay();
            }
          }
          ImGui::Separator();
          if (ImGui::MenuItem("Quit", "Ctrl+Q"))
            glfwSetWindowShouldClose(window, GLFW_TRUE);
          ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
          ImGui::MenuItem("Signals", nullptr, &state.show_signals);
          ImGui::MenuItem("Analysis", nullptr, &state.show_plotter);
          ImGui::MenuItem("Transmitter", nullptr, &state.show_transmitter);
          ImGui::MenuItem("Bus Statistics", nullptr, &state.show_statistics);
          ImGui::Separator();
          if (ImGui::BeginMenu("UI Scale")) {
            static constexpr float presets[] = {0.5f,  0.75f, 1.0f,
                                                1.25f, 1.5f,  2.0f};
            for (float p : presets) {
              auto label = std::format("{:.2g}x", p);
              bool selected = (std::abs(state.ui_scale - p) < 0.01f);
              if (ImGui::MenuItem(label.c_str(), nullptr, selected)) {
                state.ui_scale = p;
                current_scale = p;
                pending_scale = p;
              }
            }
            ImGui::EndMenu();
          }
          if (ImGui::BeginMenu("Theme")) {
            for (int i = 0; i < static_cast<int>(jcan::theme_id::count_); ++i) {
              auto tid = static_cast<jcan::theme_id>(i);
              bool sel = (state.current_theme == tid);
              if (ImGui::MenuItem(std::string(jcan::theme_name(tid)).c_str(), nullptr, sel)) {
                state.current_theme = tid;
                pending_theme = true;
              }
            }
            ImGui::EndMenu();
          }
          ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Connection")) {
          auto conn_label =
              state.connected
                  ? std::format("Connected ({} adapter{})",
                                state.adapter_slots.size(),
                                state.adapter_slots.size() > 1 ? "s" : "")
                  : std::string("Not connected");
          ImGui::TextDisabled("%s", conn_label.c_str());
          ImGui::Separator();
          if (ImGui::MenuItem("Open Connection Dialog..."))
            state.show_connection = true;
          if (state.connected) {
            if (ImGui::MenuItem("Disconnect All")) state.disconnect();
          }
          ImGui::EndMenu();
        }

        {
          static constexpr float bitrates_sb[] = {
              10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000};
          state.stats.update(bitrates_sb[std::clamp(state.selected_bitrate, 0, 8)]);

          std::string status;
          if (state.connected) {
            status += std::format("{:.0f}% | {:.0f}/s",
                                  state.stats.bus_load_pct, state.stats.total_rate_hz);
          }
          if (state.logger.recording()) {
            if (!status.empty()) status += " | ";
            status += std::format("REC {}", state.logger.frame_count());
          }
          if (state.exporting.load()) {
            if (!status.empty()) status += " | ";
            status += std::format("EXP {:.0f}%", state.export_progress.load() * 100.f);
          }
          if (state.replaying.load()) {
            if (!status.empty()) status += " | ";
            status += std::format("{} {:.0f}%",
                                  state.replay_paused.load() ? "PAUSED" : "REPLAY",
                                  state.replay_progress.load() * 100.f);
          }
          if (!status.empty()) status += " | ";
          if (state.connected) {
            status += "Connected";
            if (state.adapter_slots.size() > 1)
              status += std::format(" ({})", state.adapter_slots.size());
          } else {
            status += "Disconnected";
          }

          float status_w = ImGui::CalcTextSize(status.c_str()).x + 16.f;
          ImGui::SameLine(ImGui::GetWindowWidth() - status_w);
          ImGui::TextColored(
              state.connected ? state.colors.status_connected : state.colors.status_disconnected,
              "%s", status.c_str());
        }

        ImGui::EndMainMenuBar();
      }

      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q))
        glfwSetWindowShouldClose(window, GLFW_TRUE);
      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R) &&
          state.logger.recording()) {
        state.logger.stop();
        state.auto_start_session_log();
      }
      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E) &&
          !file_dialog.busy() && state.logger.recording() &&
          !state.exporting.load()) {
        file_dialog.save_file({{"CSV Log", "csv"}, {"Vector ASC", "asc"}},
                              "export.csv");
        pending_dialog = dialog_id::export_log;
      }
      if (state.log_mode && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O) &&
          !file_dialog.busy()) {
        pending_dbc_channel = 0xff;
        file_dialog.open_file({{"DBC Files", "dbc"}});
        pending_dialog = dialog_id::open_dbc;
      }
      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I) &&
          !file_dialog.busy()) {
        if (state.connected) {
          pending_import_confirm = true;
        } else {
          file_dialog.open_file({{"All Logs", "csv,asc,ld"},
                                 {"MoTec i2", "ld"},
                                 {"CSV / ASC", "csv,asc"}});
          pending_dialog = dialog_id::import_log;
        }
      }

      if (pending_import_confirm) {
        ImGui::OpenPopup("Import Log##confirm");
        pending_import_confirm = false;
      }
      if (ImGui::BeginPopupModal("Import Log##confirm", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Importing a log will disconnect all adapters\nand unload DBC files.");
        ImGui::Spacing();
        if (ImGui::Button("Continue", ImVec2(120, 0))) {
          state.disconnect();
          state.dbc.unload();
          file_dialog.open_file({{"All Logs", "csv,asc,ld"},
                                 {"MoTec i2", "ld"},
                                 {"CSV / ASC", "csv,asc"}});
          pending_dialog = dialog_id::import_log;
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }

      if (auto result = file_dialog.poll()) {
        switch (pending_dialog) {
          case dialog_id::open_dbc:
            if (*result) {
              if (pending_dbc_channel == 0xff) {
                auto err = state.dbc.load(**result);
                if (err.empty()) {
                  state.redecode_log();
                  state.status_text = std::format(
                      "DBC: {} msgs", state.dbc.message_ids().size());
                } else {
                  state.status_text = err;
                }
              } else {
                auto& eng = state.log_dbc[pending_dbc_channel];
                auto err = eng.load(**result);
                if (err.empty()) {
                  state.redecode_log();
                  state.status_text =
                      std::format("Ch {} DBC: {} msgs",
                                  static_cast<int>(pending_dbc_channel),
                                  eng.message_ids().size());
                } else {
                  state.status_text = err;
                }
              }
            }
            break;
          case dialog_id::open_replay:
            if (*result) {
              auto& path_str = **result;
              std::vector<std::pair<int64_t, jcan::can_frame>> frames;
              if (path_str.size() >= 4 &&
                  path_str.substr(path_str.size() - 4) == ".asc")
                frames = jcan::frame_logger::load_asc(path_str);
              else
                frames = jcan::frame_logger::load_csv(path_str);
              if (!frames.empty()) state.start_replay(std::move(frames));
            }
            break;
          case dialog_id::export_log:
            if (*result) {
              state.start_export(**result);
              state.status_text = std::format("Exporting {} frames...",
                                              state.logger.frame_count());
            }
            break;
          case dialog_id::import_log:
            if (*result) {
              auto& path_str = **result;
              auto ext = std::filesystem::path(path_str).extension().string();
              for (auto& c : ext)
                c = static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c)));

              if (ext == ".ld") {
                auto ld_result = jcan::motec::load_ld(path_str);
                if (ld_result) {
                  auto& ld = *ld_result;
                  float dur = state.import_motec(ld);

                  std::string meta;
                  if (!ld.driver.empty()) meta += ld.driver;
                  if (!ld.venue.name.empty()) {
                    if (!meta.empty()) meta += " @ ";
                    meta += ld.venue.name;
                  }
                  if (!ld.event.session.empty()) {
                    if (!meta.empty()) meta += " - ";
                    meta += ld.event.session;
                  }

                  state.status_text = std::format(
                      "MoTec: {} channels, {:.1f}s{}", ld.channels.size(), dur,
                      meta.empty() ? "" : " [" + meta + "]");

                  for (auto& c : plotter.charts) {
                    c.view_duration_sec = dur * 1.05f;
                    c.view_end_offset_sec = 0.0f;
                    c.live_follow = false;
                  }
                } else {
                  state.status_text =
                      std::format("MoTec import failed: {}", ld_result.error());
                }
              } else {
                std::vector<std::pair<int64_t, jcan::can_frame>> frames;
                if (ext == ".asc")
                  frames = jcan::frame_logger::load_asc(path_str);
                else
                  frames = jcan::frame_logger::load_csv(path_str);
                if (!frames.empty()) {
                  float dur = state.import_log(std::move(frames));
                  state.status_text =
                      std::format("Imported {} frames ({:.1f}s)",
                                  state.scrollback.size(), dur);

                  for (auto& c : plotter.charts) {
                    c.view_duration_sec = dur * 1.05f;
                    c.view_end_offset_sec = 0.0f;
                    c.live_follow = false;
                  }
                } else {
                  auto fname = std::filesystem::path(path_str).filename().string();
                  if (std::filesystem::file_size(path_str) == 0)
                    state.status_text = std::format("Import failed: {} is empty", fname);
                  else
                    state.status_text = std::format("Import failed: no valid frames in {}", fname);
                }
              }
            }
            break;
          default:
            break;
        }
        pending_dialog = dialog_id::none;
      }

      state.poll_frames();

      if (!state.exporting.load() && !state.export_result_msg.empty()) {
        state.status_text = state.export_result_msg;
        state.export_result_msg.clear();
      }

      jcan::widgets::draw_connection_panel(state);
      jcan::widgets::draw_monitor_live(state);
      jcan::widgets::draw_monitor_scrollback(state);

      if (state.show_signals) jcan::widgets::draw_signals(state);
      if (state.show_plotter) {
        jcan::widgets::draw_plotter(state, plotter);
      }
      if (state.show_transmitter) jcan::widgets::draw_transmitter(state);
      if (state.show_statistics) jcan::widgets::draw_statistics(state);

      ImGui::Render();
      int display_w, display_h;
      glfwGetFramebufferSize(window, &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(state.colors.clear_color.x, state.colors.clear_color.y,
                   state.colors.clear_color.z, state.colors.clear_color.w);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      glfwSwapBuffers(window);

      if (!focused) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    {
      settings.selected_bitrate = state.selected_bitrate;
      settings.show_signals = state.show_signals;
      settings.show_transmitter = state.show_transmitter;
      settings.show_statistics = state.show_statistics;
      settings.show_plotter = state.show_plotter;
      settings.ui_scale = state.ui_scale;
      settings.theme = static_cast<int>(state.current_theme);
      settings.log_dir = state.log_dir.string();
      settings.dbc_paths = state.dbc.paths();
      if (!state.adapter_slots.empty())
        settings.last_adapter_port = state.adapter_slots[0]->desc.port;
      int w, h;
      glfwGetWindowSize(window, &w, &h);
      settings.window_width = w;
      settings.window_height = h;
      settings.save();
    }

    state.export_thread.reset();
    state.logger.stop();
    state.disconnect();

    g_drop_state = nullptr;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    NFD_Quit();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
  } catch (const std::exception& e) {
#ifdef _WIN32
    MessageBoxA(nullptr, e.what(), "jcan - Fatal Error", MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "Fatal: %s\n", e.what());
#endif
    return 1;
  } catch (...) {
#ifdef _WIN32
    MessageBoxA(nullptr, "Unknown fatal error", "jcan - Fatal Error",
                MB_OK | MB_ICONERROR);
#else
    std::fprintf(stderr, "Fatal: unknown error\n");
#endif
    return 1;
  }
}
