#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <nfd.h>

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <thread>

#include "app_state.hpp"
#include "discovery.hpp"
#include "logger.hpp"
#include "settings.hpp"
#include "widgets/connection.hpp"
#include "widgets/monitor.hpp"
#include "widgets/signal_watcher.hpp"
#include "widgets/statistics.hpp"
#include "widgets/transmitter.hpp"

static void glfw_error_callback(int error, const char* description) {
  std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static jcan::app_state* g_drop_state = nullptr;

static void glfw_drop_callback([[maybe_unused]] GLFWwindow* window, int count,
                               const char** paths) {
  if (!g_drop_state || count <= 0) return;
  for (int i = 0; i < count; ++i) {
    std::string path(paths[i]);
    if (path.size() >= 4) {
      std::string ext = path.substr(path.size() - 4);
      for (auto& c : ext) c = static_cast<char>(std::tolower(c));
      if (ext == ".dbc") {
        if (g_drop_state->dbc.load(path)) {
          g_drop_state->last_dbc_path = path;
          g_drop_state->status_text =
              std::format("DBC: {} ({} msgs)", g_drop_state->dbc.filename(),
                          g_drop_state->dbc.message_ids().size());
        } else {
          g_drop_state->status_text = "DBC load failed!";
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

  ImGuiID left, right;
  ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.25f, &left,
                              &right);

  ImGuiID right_top, right_bottom;
  ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.55f, &right_top,
                              &right_bottom);

  ImGuiID bot_left, bot_right;
  ImGui::DockBuilderSplitNode(right_bottom, ImGuiDir_Left, 0.60f, &bot_left,
                              &bot_right);

  ImGui::DockBuilderDockWindow("Connection", left);
  ImGui::DockBuilderDockWindow("Bus Monitor - Live", right_top);
  ImGui::DockBuilderDockWindow("Bus Monitor - Scrollback", bot_left);
  ImGui::DockBuilderDockWindow("Bus Statistics", bot_left);
  ImGui::DockBuilderDockWindow("Signal Watcher", bot_right);
  ImGui::DockBuilderDockWindow("Transmitter", bot_right);

  ImGui::DockBuilderFinish(dockspace_id);
}

static void open_dbc_dialog(jcan::app_state& state) {
  nfdu8char_t* out_path = nullptr;
  nfdu8filteritem_t filters[] = {{"DBC Files", "dbc"}};
  nfdresult_t result = NFD_OpenDialogU8(&out_path, filters, 1, nullptr);

  if (result == NFD_OKAY && out_path) {
    std::string path_str(out_path);
    if (state.dbc.load(path_str)) {
      state.last_dbc_path = path_str;
      state.status_text = std::format("DBC: {} ({} msgs)", state.dbc.filename(),
                                      state.dbc.message_ids().size());
    } else {
      state.status_text = "DBC load failed!";
    }
    NFD_FreePathU8(out_path);
  }
}

int main() {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) return 1;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  NFD_Init();

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

  ImGui::StyleColorsDark();

  ImGuiStyle& style = ImGui::GetStyle();
  style.FrameRounding = 4.0f;
  style.GrabRounding = 3.0f;
  style.WindowRounding = 6.0f;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  ImFont* mono_font = nullptr;
  {
    const char* mono_paths[] = {
        "/usr/share/fonts/TTF/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/truetype/jetbrains-mono/JetBrainsMono-Regular.ttf",
        "/usr/share/fonts/TTF/FiraCode-Regular.ttf",
        "/usr/share/fonts/truetype/firacode/FiraCode-Regular.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        nullptr,
    };
    for (const char** p = mono_paths; *p; ++p) {
      if (std::filesystem::exists(*p)) {
        mono_font = io.Fonts->AddFontFromFileTTF(*p, 14.0f);
        if (mono_font) break;
      }
    }
    io.Fonts->Build();
  }

  jcan::app_state state;
  state.mono_font = mono_font;

  state.selected_bitrate = settings.selected_bitrate;
  state.show_signal_watcher = settings.show_signal_watcher;
  state.show_transmitter = settings.show_transmitter;
  state.show_statistics = settings.show_statistics;

  state.devices = jcan::discover_adapters();

  if (!settings.last_adapter_port.empty()) {
    for (int i = 0; i < static_cast<int>(state.devices.size()); ++i) {
      if (state.devices[i].port == settings.last_adapter_port) {
        state.selected_device = i;
        break;
      }
    }
  }

  if (!settings.last_dbc_path.empty() &&
      std::filesystem::exists(settings.last_dbc_path)) {
    if (state.dbc.load(settings.last_dbc_path)) {
      state.last_dbc_path = settings.last_dbc_path;
      state.status_text = std::format("DBC: {} ({} msgs)", state.dbc.filename(),
                                      state.dbc.message_ids().size());
    }
  }

  g_drop_state = &state;
  glfwSetDropCallback(window, glfw_drop_callback);

  bool first_frame = true;
  bool was_focused = true;

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

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(
        0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    if (first_frame) {
      setup_default_layout(dockspace_id);
      first_frame = false;
    }

    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Load DBC...", "Ctrl+O")) open_dbc_dialog(state);
        if (state.dbc.loaded()) {
          if (ImGui::MenuItem("Unload DBC")) {
            state.dbc.unload();
            state.last_dbc_path.clear();
            state.signal_watcher.traces.clear();
            state.status_text =
                state.connected ? "DBC unloaded" : "Disconnected";
          }
        }
        ImGui::Separator();
        if (!state.logger.recording()) {
          if (ImGui::MenuItem("Start Recording (CSV)...", "Ctrl+R")) {
            nfdu8char_t* out_path = nullptr;
            nfdu8filteritem_t filters[] = {{"CSV Log", "csv"}};
            if (NFD_SaveDialogU8(&out_path, filters, 1, nullptr,
                                 "capture.csv") == NFD_OKAY &&
                out_path) {
              state.logger.start(out_path);
              NFD_FreePathU8(out_path);
            }
          }
          if (ImGui::MenuItem("Start Recording (ASC)...")) {
            nfdu8char_t* out_path = nullptr;
            nfdu8filteritem_t filters[] = {{"Vector ASC", "asc"}};
            if (NFD_SaveDialogU8(&out_path, filters, 1, nullptr,
                                 "capture.asc") == NFD_OKAY &&
                out_path) {
              state.logger.start(out_path);
              NFD_FreePathU8(out_path);
            }
          }
        } else {
          auto rec_label = std::format("Stop Recording ({} frames)",
                                       state.logger.frame_count());
          if (ImGui::MenuItem(rec_label.c_str())) {
            state.logger.stop();
          }
        }
        if (!state.replaying.load()) {
          if (ImGui::MenuItem("Replay Log...")) {
            nfdu8char_t* out_path = nullptr;
            nfdu8filteritem_t filters[] = {{"CSV / ASC Log", "csv,asc"}};
            if (NFD_OpenDialogU8(&out_path, filters, 1, nullptr) == NFD_OKAY &&
                out_path) {
              std::string path_str(out_path);
              NFD_FreePathU8(out_path);
              std::vector<std::pair<int64_t, jcan::can_frame>> frames;
              if (path_str.size() >= 4 &&
                  path_str.substr(path_str.size() - 4) == ".asc")
                frames = jcan::frame_logger::load_asc(path_str);
              else
                frames = jcan::frame_logger::load_csv(path_str);
              if (!frames.empty()) {
                state.start_replay(std::move(frames));
              }
            }
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

          auto replay_label = std::format("Stop Replay ({:.0f}%)",
                                          state.replay_progress.load() * 100.f);
          if (ImGui::MenuItem(replay_label.c_str())) {
            state.stop_replay();
          }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Connection...", nullptr,
                            state.show_connection_modal))
          state.show_connection_modal = !state.show_connection_modal;
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q"))
          glfwSetWindowShouldClose(window, GLFW_TRUE);
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Signal Watcher", nullptr, &state.show_signal_watcher);
        ImGui::MenuItem("Transmitter", nullptr, &state.show_transmitter);
        ImGui::MenuItem("Bus Statistics", nullptr, &state.show_statistics);
        ImGui::Separator();
        ImGui::MenuItem("Demo Window", nullptr, &state.show_demo_window);
        ImGui::EndMenu();
      }

      ImGui::SameLine();
      if (state.connected) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        if (ImGui::SmallButton("[x] Disconnect")) state.disconnect();
        ImGui::PopStyleColor();
      } else if (!state.devices.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 1.0f, 0.3f, 1.0f));
        if (ImGui::SmallButton("[>] Connect")) state.connect();
        ImGui::PopStyleColor();
      }

      std::string full_status;
      if (state.logger.recording())
        full_status += std::format("[REC:{}] ", state.logger.frame_count());
      if (state.replaying.load()) {
        bool paused = state.replay_paused.load();
        float speed = state.replay_speed.load();
        full_status += std::format(
            "[REPLAY:{:.0f}%{}{}] ", state.replay_progress.load() * 100.f,
            paused ? " PAUSED" : "",
            speed != 1.0f ? std::format(" {:.2g}x", speed) : "");
      }
      if (state.dbc.loaded())
        full_status += std::format("[{}] ", state.dbc.filename());
      full_status += state.status_text;

      float status_width = ImGui::CalcTextSize(full_status.c_str()).x + 16;
      ImGui::SameLine(ImGui::GetWindowWidth() - status_width);
      if (state.connected)
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s",
                           full_status.c_str());
      else
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                           full_status.c_str());

      ImGui::EndMainMenuBar();
    }

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) open_dbc_dialog(state);
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q))
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_R)) {
      if (!state.logger.recording()) {
        nfdu8char_t* out_path = nullptr;
        nfdu8filteritem_t filters[] = {{"CSV Log", "csv"}};
        if (NFD_SaveDialogU8(&out_path, filters, 1, nullptr, "capture.csv") ==
                NFD_OKAY &&
            out_path) {
          state.logger.start(out_path);
          NFD_FreePathU8(out_path);
        }
      } else {
        state.logger.stop();
      }
    }

    state.poll_frames();

    jcan::widgets::draw_connection_modal(state);
    jcan::widgets::draw_monitor_live(state);
    jcan::widgets::draw_monitor_scrollback(state);

    if (state.show_signal_watcher) jcan::widgets::draw_signal_watcher(state);
    if (state.show_transmitter) jcan::widgets::draw_transmitter(state);
    if (state.show_statistics) jcan::widgets::draw_statistics(state);
    if (state.show_demo_window) ImGui::ShowDemoWindow(&state.show_demo_window);

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);

    if (!focused) std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  {
    settings.selected_bitrate = state.selected_bitrate;
    settings.show_signal_watcher = state.show_signal_watcher;
    settings.show_transmitter = state.show_transmitter;
    settings.show_statistics = state.show_statistics;
    settings.last_dbc_path = state.last_dbc_path;
    if (!state.adapter_slots.empty())
      settings.last_adapter_port = state.adapter_slots[0]->desc.port;
    int w, h;
    glfwGetWindowSize(window, &w, &h);
    settings.window_width = w;
    settings.window_height = h;
    settings.save();
  }

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
}
