#pragma once

#include <rex/rex_app.h>
#include <rex/cvar.h>
#include <rex/chrono/clock.h>
#include <rex/filesystem.h>
#include <rex/input/flags.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/graphics/command_processor.h>
#include <rex/graphics/graphics_system.h>
#include <rex/logging/macros.h>

#include "dantes_inferno_hooks.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_manager.h>

#include "native_renderer/native_presenter.h"

#ifdef DANTESINFERNO_NATIVE_RENDERER
#include <memory>
#endif

REXCVAR_DEFINE_DOUBLE(time_scalar, 1.0, "Gameplay",
                      "Guest time scaling factor (1.0 = normal, 50.0 = fast-forward)");

REXCVAR_DEFINE_BOOL(show_fps_overlay, false, "UI",
                    "Show FPS and frametime overlay (top-left corner)");

REXCVAR_DEFINE_STRING(glyph_family, "auto", "UI",
                      "Button glyph family: auto, xbox, or playstation");

REXCVAR_DEFINE_DOUBLE(ultrawide_target_aspect, 0.0, "Graphics",
                      "Target aspect ratio for ultrawide (0=disabled, 1.7778=16:9, "
                      "2.3889=21:9, 3.5556=32:9)");

REXCVAR_DEFINE_DOUBLE(engine_base_hz_override, 0.0, "Diagnostics",
                      "Override guest base Hz timing constant (0 = disabled). "
                      "Distinct from the SDK target_fps diagnostic cvar.")
    .range(0.0, 240.0)
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_BOOL(ui_timing_fix, false, "Diagnostics/Experimental",
                    "Present a local 60Hz compatibility clock to verified UI timing "
                    "consumers only. Requires engine_base_hz_override>0. Requires restart.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_BOOL(catching_souls_timing_fix, false, "Diagnostics/Experimental",
                    "Scale only the Catching Souls prompt's per-frame counter increment "
                    "to 60Hz time. Requires engine_base_hz_override>0. Requires restart.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_BOOL(dlc_trace, false, "Diagnostics",
                    "Trace DLC module reads, guest callers, and activation (requires restart).")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_BOOL(dlc_dump_image, false, "Diagnostics",
                    "Dump the loaded guest image for offline DLC analysis (requires restart).")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_STRING(dlc_source_path, "dlc", "Content",
                      "Folder scanned for DLC packages to auto-install on launch. "
                      "If empty or missing, the game runs without DLC.");

class FpsOverlayDialog : public rex::ui::ImGuiDialog {
 public:
  explicit FpsOverlayDialog(rex::ui::ImGuiDrawer* drawer,
                            rex::graphics::CommandProcessor* command_processor)
      : rex::ui::ImGuiDialog(drawer),
        command_processor_(command_processor),
        last_time_(std::chrono::steady_clock::now()),
        last_guest_frame_count_(command_processor ? command_processor->counter() : 0) {}

 protected:
  void OnDraw(ImGuiIO& io) override {
    auto now = std::chrono::steady_clock::now();
    auto delta = std::chrono::duration<double, std::milli>(now - last_time_);
    last_time_ = now;

    uint64_t current_guest_frames =
        command_processor_ ? command_processor_->counter() : 0;
    uint64_t frames_delta = current_guest_frames - last_guest_frame_count_;
    last_guest_frame_count_ = current_guest_frames;

    double interval_ms = delta.count();
    double guest_fps = 0;
    double guest_ft_ms = 0;
    if (frames_delta > 0 && interval_ms > 0) {
      guest_fps = frames_delta * 1000.0 / interval_ms;
      guest_ft_ms = interval_ms / frames_delta;
    }

    frame_history_[history_idx_] = static_cast<float>(guest_ft_ms);
    history_idx_ = (history_idx_ + 1) % kHistorySize;

    if (smoothed_fps_ == 0.0) {
      smoothed_fps_ = guest_fps;
      smoothed_ft_ = guest_ft_ms;
    } else {
      smoothed_fps_ = smoothed_fps_ * 0.85 + guest_fps * 0.15;
      smoothed_ft_ = smoothed_ft_ * 0.85 + guest_ft_ms * 0.15;
    }

    ImGui::SetNextWindowPos(ImVec2(8, 8), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);

    bool visible = true;
    if (ImGui::Begin("##fps_overlay", &visible,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoFocusOnAppearing)) {
      ImGui::SetWindowFontScale(2.0f);
      ImU32 fps_color = smoothed_fps_ >= 55.0f ? IM_COL32(80, 255, 80, 255) :
                       smoothed_fps_ >= 30.0f ? IM_COL32(255, 220, 60, 255) :
                                                IM_COL32(255, 80, 80, 255);
      ImGui::PushStyleColor(ImGuiCol_Text, fps_color);
      ImGui::Text("%.0f FPS", smoothed_fps_);
      ImGui::PopStyleColor();
      ImGui::SetWindowFontScale(1.0f);

      ImGui::SetWindowFontScale(1.3f);
      ImGui::Text("%.1f ms", smoothed_ft_);
      ImGui::SetWindowFontScale(1.0f);

      ImGui::Spacing();
      ImGui::PlotLines("##frametime", frame_history_.data(),
                       static_cast<int>(kHistorySize),
                       static_cast<int>(history_idx_), nullptr,
                       0.0f, 50.0f, ImVec2(220, 50));
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
  }

 private:
  static constexpr size_t kHistorySize = 120;
  std::array<float, kHistorySize> frame_history_{};
  size_t history_idx_ = 0;
  rex::graphics::CommandProcessor* command_processor_;
  std::chrono::steady_clock::time_point last_time_;
  uint64_t last_guest_frame_count_;
  double smoothed_fps_ = 0.0;
  double smoothed_ft_ = 0.0;
};

class DantesInfernoApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<DantesInfernoApp>(new DantesInfernoApp(ctx, "dantes_inferno",
        PPCImageConfig));
  }

  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (!paths.game_data_root.empty())
      return;

    const std::filesystem::path candidates[] = {
        rex::filesystem::GetExecutableFolder() / "game",
        std::filesystem::current_path() / "game",
    };
    for (const auto& candidate : candidates) {
      std::error_code ec;
      if (std::filesystem::is_directory(candidate, ec)) {
        paths.game_data_root = candidate;
        return;
      }
    }
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    config.gpu_plugin = "xenos";

    REXCVAR_SET(input_backend, std::string("sdl"));

    if (REXCVAR_GET(engine_base_hz_override) <= 0.0) {
      REXCVAR_SET(engine_base_hz_override, 120.0);
    }
    REXCVAR_SET(ui_timing_fix, true);
    REXCVAR_SET(catching_souls_timing_fix, true);

    const double override_hz = REXCVAR_GET(engine_base_hz_override);
    if (override_hz > 0.0) {
      rex::cvar::SetFlagByName("video_mode_refresh_rate", std::to_string(override_hz));
      rex::cvar::SetFlagByName("target_fps", "0");
      rex::cvar::SetFlagByName("vsync", "true");
    }

    rex::cvar::SetFlagByName("mnk_mode", "true");
    rex::cvar::SetFlagByName("mnk_mouse", "true");
    rex::cvar::SetFlagByName("mnk_sensitivity", "1.5");

    rex::cvar::SetFlagByName("keybind_a", "Space");
    rex::cvar::SetFlagByName("keybind_b", "F");
    rex::cvar::SetFlagByName("keybind_x", "MouseLeft");
    rex::cvar::SetFlagByName("keybind_y", "E");
    rex::cvar::SetFlagByName("keybind_left_shoulder", "Q");
    rex::cvar::SetFlagByName("keybind_right_shoulder", "MouseRight");
    rex::cvar::SetFlagByName("keybind_left_trigger", "Shift");
    rex::cvar::SetFlagByName("keybind_right_trigger", "Ctrl");
    rex::cvar::SetFlagByName("keybind_lstick_up", "W");
    rex::cvar::SetFlagByName("keybind_lstick_down", "S");
    rex::cvar::SetFlagByName("keybind_lstick_left", "A");
    rex::cvar::SetFlagByName("keybind_lstick_right", "D");
    rex::cvar::SetFlagByName("keybind_lstick_press", "X");
    rex::cvar::SetFlagByName("keybind_rstick_up", "Up");
    rex::cvar::SetFlagByName("keybind_rstick_down", "Down");
    rex::cvar::SetFlagByName("keybind_rstick_left", "Left");
    rex::cvar::SetFlagByName("keybind_rstick_right", "Right");
    rex::cvar::SetFlagByName("keybind_rstick_press", "R");
    rex::cvar::SetFlagByName("keybind_dpad_up", "Shift+Up");
    rex::cvar::SetFlagByName("keybind_dpad_down", "Shift+Down");
    rex::cvar::SetFlagByName("keybind_dpad_left", "Shift+Left");
    rex::cvar::SetFlagByName("keybind_dpad_right", "Shift+Right");
    rex::cvar::SetFlagByName("keybind_back", "Tab");
    rex::cvar::SetFlagByName("keybind_start", "Escape");

    double target_aspect = REXCVAR_GET(ultrawide_target_aspect);
    if (target_aspect > 0.0) {
      g_ultrawide_target_aspect = static_cast<float>(target_aspect);

      uint32_t target_width = static_cast<uint32_t>(target_aspect * 720.0 + 0.5);
      uint32_t video_mode_w = std::min(target_width, 4095u);
      uint32_t window_w = std::min(target_width, 8192u);
      rex::cvar::SetFlagByName("video_mode_width", std::to_string(video_mode_w));
      rex::cvar::SetFlagByName("video_mode_height", "720");
      rex::cvar::SetFlagByName("window_width", std::to_string(window_w));
      rex::cvar::SetFlagByName("window_height", "720");

      REXLOG_INFO(
          "ULTRAWIDE: target_aspect={:.4f}, window={}x720, video_mode={}x720, "
          "present_letterbox={}",
          target_aspect, window_w, video_mode_w,
          rex::cvar::Query<bool>("present_letterbox"));
    } else {
      REXLOG_INFO("ULTRAWIDE: disabled (target_aspect={:.4f})", target_aspect);
    }
  }

  void OnPreLaunchModule() override {
    uint8_t* membase = runtime()->memory()->virtual_membase();

    auto* ptr = reinterpret_cast<uint32_t*>(membase + 0x82B101E4);
    *ptr = 0u;

    g_dlc_trace_enabled = REXCVAR_GET(dlc_trace);
    REXLOG_INFO("DLC-TRACE: {}", g_dlc_trace_enabled ? "enabled" : "disabled");

    if (REXCVAR_GET(dlc_dump_image)) {
      FILE* f = fopen("out\\build\\win-amd64-release\\logs\\guest_image.bin", "wb");
      if (f) {
        fwrite(membase + 0x82000000, 1, 0xD70000, f);
        fclose(f);
        REXLOG_INFO("DLC-MOD: dumped guest image");
      }
    }
  }

  void AutoInstallDlc() {
    auto* kernel_state = runtime() ? runtime()->kernel_state() : nullptr;
    auto* content_manager = kernel_state ? kernel_state->content_manager() : nullptr;
    if (!content_manager) {
      REXLOG_WARN("DLC auto-install skipped: content manager unavailable");
      return;
    }

    std::string dlc_path = REXCVAR_GET(dlc_source_path);
    if (dlc_path.empty()) dlc_path = "dlc";
    std::filesystem::path root(dlc_path);
    if (!std::filesystem::exists(root)) {
      REXLOG_INFO("DLC folder not found ({}); running without DLC", dlc_path);
      return;
    }

    std::filesystem::path marker = root / ".installed";
    std::set<std::string> installed;
    if (std::filesystem::exists(marker)) {
      std::ifstream in(marker);
      std::string line;
      while (std::getline(in, line)) {
        if (!line.empty()) installed.insert(line);
      }
    }

    std::vector<std::filesystem::path> packages;
    for (auto& entry : std::filesystem::recursive_directory_iterator(root)) {
      if (entry.is_regular_file()) {
        auto name = entry.path().filename().string();
        if (name == ".installed") continue;
        packages.push_back(entry.path());
      }
    }

    if (packages.empty()) {
      REXLOG_INFO("DLC folder empty ({}); running without DLC", dlc_path);
      return;
    }

    int new_installed = 0;
    for (auto& pkg : packages) {
      auto rel = std::filesystem::relative(pkg, root).string();
      if (installed.count(rel)) continue;

      REXLOG_INFO("Installing DLC package: {}", rel);
      auto result = content_manager->InstallContent(pkg);
      if (XSUCCEEDED(result)) {
        installed.insert(rel);
        new_installed++;
        std::ofstream out(marker, std::ios::app);
        out << rel << "\n";
      } else {
        REXLOG_WARN("DLC install failed for {}: 0x{:08X}", rel, result);
      }
    }

    REXLOG_INFO("DLC auto-install complete: {} new, {} total", new_installed,
                installed.size());
  }

  void OnPostSetup() override {
    AutoInstallDlc();

    rex::chrono::Clock::set_guest_time_scalar(REXCVAR_GET(time_scalar));

    rex::cvar::RegisterChangeCallback("time_scalar",
        [](std::string_view, std::string_view new_value) {
          double scalar = std::stod(std::string(new_value));
          if (scalar < 0.0) scalar = 0.0;
          rex::chrono::Clock::set_guest_time_scalar(scalar);
        });

    rex::cvar::RegisterChangeCallback("ultrawide_target_aspect",
        [](std::string_view, std::string_view new_value) {
          double aspect = std::stod(std::string(new_value));
          g_ultrawide_target_aspect = static_cast<float>(aspect);
        });

    rex::ui::RegisterBind("bind_exit_game", "Alt+F4",
                          "Exit game to desktop", [this] {
      app_context().RequestDeferredQuit();
    });

    if (REXCVAR_GET(show_fps_overlay) && imgui_drawer()) {
      auto* gfx_sys = runtime() ? runtime()->graphics_system() : nullptr;
      auto* command_processor = gfx_sys
          ? static_cast<rex::graphics::GraphicsSystem*>(gfx_sys)->command_processor()
          : nullptr;
      fps_overlay_ = std::make_unique<FpsOverlayDialog>(imgui_drawer(), command_processor);
    }

    rex::ui::RegisterBind("bind_fps_overlay", "F1",
                          "Toggle FPS overlay", [this] {
      if (fps_overlay_) {
        fps_overlay_.reset();
      } else if (imgui_drawer()) {
        auto* gfx_sys = runtime() ? runtime()->graphics_system() : nullptr;
        auto* command_processor = gfx_sys
            ? static_cast<rex::graphics::GraphicsSystem*>(gfx_sys)->command_processor()
            : nullptr;
        fps_overlay_ = std::make_unique<FpsOverlayDialog>(imgui_drawer(), command_processor);
      }
    });

    diag_stop_.store(false);
    diag_thread_ = std::make_unique<std::thread>(&DantesInfernoApp::DiagnosticsLogger, this);

    rex::ui::RegisterBind("bind_fast_forward", "F2",
                          "Toggle 50x fast-forward", [this] {
      double current = REXCVAR_GET(time_scalar);
      bool fast = current > 1.0;
      double target = fast ? 1.0 : 50.0;
      rex::cvar::SetFlagByName("time_scalar", std::to_string(target));
      rex::chrono::Clock::set_guest_time_scalar(target);
      rex::cvar::SetFlagByName("vsync", fast ? "true" : "false");
    });

#ifdef DANTESINFERNO_NATIVE_RENDERER
    if (rex::cvar::Query<bool>("use_native_presenter")) {
      auto* gfx_sys = runtime() ? runtime()->graphics_system() : nullptr;
      auto* graphics_system = static_cast<rex::graphics::GraphicsSystem*>(gfx_sys);
      auto* rex_presenter = graphics_system ? graphics_system->presenter() : nullptr;
      if (rex_presenter && window()) {
        native_presenter_ = std::make_unique<dante::NativePresenter>();
        uint32_t w = window()->GetDesiredLogicalWidth();
        uint32_t h = window()->GetDesiredLogicalHeight();
        if (w == 0) w = 1280;
        if (h == 0) h = 720;
        if (!native_presenter_->initialize(rex_presenter, window(), w, h)) {
          REXLOG_WARN("Native presenter init failed; staying on D3D12 path");
          native_presenter_.reset();
        }
      } else {
        REXLOG_WARN("use_native_presenter=1 but presenter/window unavailable");
      }
    }
#endif
  }

  void OnShutdown() override {
    diag_stop_.store(true, std::memory_order_relaxed);
    if (diag_thread_ && diag_thread_->joinable()) {
      diag_thread_->join();
      diag_thread_.reset();
    }

#ifdef DANTESINFERNO_NATIVE_RENDERER
    native_presenter_.reset();
#endif

    rex::ui::UnregisterBind("bind_fast_forward");
    rex::ui::UnregisterBind("bind_fps_overlay");
    rex::ui::UnregisterBind("bind_exit_game");
    rex::cvar::UnregisterChangeCallbacks("time_scalar");
    rex::cvar::UnregisterChangeCallbacks("ultrawide_target_aspect");
    fps_overlay_.reset();
    rex::chrono::Clock::set_guest_time_scalar(1.0);
    rex::cvar::SetFlagByName("vsync", "true");
  }

 private:
  std::unique_ptr<FpsOverlayDialog> fps_overlay_;
#ifdef DANTESINFERNO_NATIVE_RENDERER
  std::unique_ptr<dante::NativePresenter> native_presenter_;
#endif
  std::atomic<bool> diag_stop_{false};
  std::unique_ptr<std::thread> diag_thread_;

  void DiagnosticsLogger() {
    DiagSnapshot prev{};
    DiagSnapshot cur{};
    g_diag_counters.Snapshot(prev);
    uint64_t prev_guest_frames = 0;
    auto start = std::chrono::steady_clock::now();
    auto last_log = start;
    while (!diag_stop_.load(std::memory_order_relaxed)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      auto now = std::chrono::steady_clock::now();
      double elapsed = std::chrono::duration<double>(now - last_log).count();
      if (elapsed < 5.0) continue;
      g_diag_counters.Snapshot(cur);
      uint64_t guest_frames = 0;
      double guest_fps = 0.0;
      auto* gfx_sys = runtime() ? runtime()->graphics_system() : nullptr;
      auto* cp = gfx_sys
          ? static_cast<rex::graphics::GraphicsSystem*>(gfx_sys)->command_processor()
          : nullptr;
      if (cp) {
        guest_frames = cp->counter();
        if (prev_guest_frames > 0 && guest_frames > prev_guest_frames) {
          guest_fps = double(guest_frames - prev_guest_frames) / elapsed;
        }
      }
      dante::NativePresenterMetrics np{};
#ifdef DANTESINFERNO_NATIVE_RENDERER
      if (native_presenter_) np = native_presenter_->GetMetrics();
#endif
      auto delta = [](uint64_t c, uint64_t p) { return c >= p ? c - p : uint64_t(0); };
      auto rate = [&](uint64_t c, uint64_t p) { return double(delta(c, p)) / elapsed; };
      uint64_t scheduler_window_count = delta(cur.scheduler_message_overflow_count,
                                              prev.scheduler_message_overflow_count);
      for (size_t i = 0; i < cur.scheduler_message_counts.size(); ++i) {
        scheduler_window_count += delta(cur.scheduler_message_counts[i],
                                        prev.scheduler_message_counts[i]);
      }
      REXLOG_INFO(
          "DIAG: t={:.1f}s guest_fps={:.2f} "
          "vblank_cb={:.1f}/s (src={:08X},arg={:08X}) vblank_ev={:.1f}/s "
          "cb4170={:.1f}/s (tgt={:08X}) cb416c_val={:.1f}/s cb416c_call={:.1f}/s (tgt={:08X}) "
          "f324={:.1f}/s 5504={:08X} d_f3bc={} d_f3c8_clamped={} d_f3cc_final={} "
          "hb_reader={:.1f}/s hb_pub={:08X}@{:.1f}/s hb_cons={:08X}@{:.1f}/s hb_bound={:.1f}/s "
          "cb_sched={:.1f}/s cb_slots={:.1f}/s cb_dispatch[slot={:X},obj={:08X},arg={:08X}]={:.1f}/s "
          "ui_dispatch={:.1f}/s vdswap={:.1f}/s "
          "render_entry={:.1f}/s render_loop={:.1f}/s "
          "scheduler={:.1f}/s overflow={:.1f}/s msg_ptr={:08X} f1={:.6g} f2={:.6g} f3={:.6g} "
          "present_loop={:.1f}/s async={:08X} "
          "np=[gf={} ok={}/{} fail={}/{} empty={} mode={}]",
          std::chrono::duration<double>(now - start).count(), guest_fps,
          rate(cur.vblank_callback_count, prev.vblank_callback_count),
          cur.vblank_callback_source, cur.vblank_callback_arg,
          rate(cur.vblank_event_processor_count, prev.vblank_event_processor_count),
          rate(cur.callback_4170_count, prev.callback_4170_count),
          cur.callback_4170_target,
          rate(cur.callback_416c_value_count, prev.callback_416c_value_count),
          rate(cur.callback_416c_call_count, prev.callback_416c_call_count),
          cur.callback_416c_target,
          rate(cur.f324_entry_count, prev.f324_entry_count), cur.value_5504,
          cur.delta_f3bc, cur.delta_f3c8, cur.final_delta,
          rate(cur.hb_reader_entry_count, prev.hb_reader_entry_count),
          cur.hb_published_value, rate(cur.hb_published_count, prev.hb_published_count),
          cur.hb_consumed_value, rate(cur.hb_consumed_count, prev.hb_consumed_count),
          rate(cur.hb_call_boundary_count, prev.hb_call_boundary_count),
          rate(cur.cb_sched_entry_count, prev.cb_sched_entry_count),
          rate(cur.cb_slot_iter_entry_count, prev.cb_slot_iter_entry_count),
          cur.cb_dispatch_slot, cur.cb_dispatch_object, cur.cb_dispatch_arg,
          rate(cur.cb_dispatch_entry_count, prev.cb_dispatch_entry_count),
          rate(cur.engine_ui_dispatch_count, prev.engine_ui_dispatch_count),
          rate(cur.vdswap_count, prev.vdswap_count),
          rate(cur.render_loop_entry_count, prev.render_loop_entry_count),
          rate(cur.render_loop_loopback_count, prev.render_loop_loopback_count),
          double(scheduler_window_count) / elapsed,
          rate(cur.scheduler_message_overflow_count, prev.scheduler_message_overflow_count),
          cur.scheduler_message_pointer,
          std::bit_cast<double>(cur.scheduler_message_f1),
          std::bit_cast<double>(cur.scheduler_message_f2),
          std::bit_cast<double>(cur.scheduler_message_f3),
          rate(cur.presentation_loop_count, prev.presentation_loop_count),
          cur.presentation_async_flag,
          np.guest_frame_count, np.capture_ok, np.interop_ok, np.capture_fail,
          np.interop_fail, np.empty_image,
          (np.gpu_interop_active && !np.gpu_interop_failed) ? "gpu" : "cpu");
      for (size_t i = 0; i < cur.scheduler_message_ids.size(); ++i) {
        if (cur.scheduler_message_ids[i] != UINT32_MAX) {
          REXLOG_INFO("DIAG_MSG: {:08X}:{:.1f}/s", cur.scheduler_message_ids[i],
                      rate(cur.scheduler_message_counts[i], prev.scheduler_message_counts[i]));
        }
      }
      std::ostringstream sdbm;
      bool sdbm_first = true;
      for (size_t i = 0; i < kSdbmMessages.size(); ++i) {
        uint64_t d = delta(cur.sdbm_message_counts[i], prev.sdbm_message_counts[i]);
        if (d > 0) {
          if (!sdbm_first) sdbm << " ";
          sdbm << kSdbmMessages[i].name << "=" << std::fixed << std::setprecision(1)
               << (double(d) / elapsed) << "/s";
          sdbm_first = false;
        }
      }
      uint64_t sdbm_unknown = delta(cur.sdbm_message_unknown_count,
                                    prev.sdbm_message_unknown_count);
      if (!sdbm_first) sdbm << " ";
      sdbm << "unknown_total=" << sdbm_unknown;
      REXLOG_INFO("DIAG_SDBM: latest=0x{:08X} payload=0x{:08X} context=0x{:08X} {}",
                  cur.sdbm_latest_id, cur.sdbm_latest_payload, cur.sdbm_latest_context,
                  sdbm.str());
      for (size_t i = 0; i < cur.sdbm_unknown_ids.size(); ++i) {
        if (cur.sdbm_unknown_ids[i] != UINT32_MAX) {
          REXLOG_INFO("DIAG_SDBM_UNKNOWN: {:08X}:{:.1f}/s",
                      cur.sdbm_unknown_ids[i],
                      rate(cur.sdbm_unknown_counts[i], prev.sdbm_unknown_counts[i]));
        }
      }
      uint64_t handler_overflow = delta(cur.sdbm_handler_overflow_count,
                                        prev.sdbm_handler_overflow_count);
      if (handler_overflow > 0) {
        REXLOG_INFO("DIAG_HANDLER_OVERFLOW: +{} in window", handler_overflow);
      }
      for (size_t i = 0; i < cur.sdbm_handler_ids.size(); ++i) {
        if (cur.sdbm_handler_ids[i] == UINT32_MAX) continue;
        uint64_t handler_window = delta(cur.sdbm_handler_counts[i],
                                        prev.sdbm_handler_counts[i]);
        if (handler_window == 0) continue;
        REXLOG_INFO(
            "DIAG_HANDLER id={:08X} producer={:08X} target={:08X} obj={:08X} rate={:.1f}/s",
            cur.sdbm_handler_ids[i], cur.sdbm_handler_producers[i],
            cur.sdbm_handler_targets[i], cur.sdbm_handler_objs[i],
            double(handler_window) / elapsed);
      }
      REXLOG_INFO("DIAG_MODE: value={} transitions={:.1f}/s object=0x{:08X}",
                  cur.mode_byte_value,
                  rate(cur.mode_byte_transitions, prev.mode_byte_transitions),
                  cur.mode_byte_object);
      REXLOG_INFO("DIAG_130: A={:.1f}/s val={:.6g} obj=0x{:08X} "
                  "B={:.1f}/s val={:.6g} obj=0x{:08X}",
                  rate(cur.onethirty_a_count, prev.onethirty_a_count),
                  cur.onethirty_a_value, cur.onethirty_a_object,
                  rate(cur.onethirty_b_count, prev.onethirty_b_count),
                  cur.onethirty_b_value, cur.onethirty_b_object);
      REXLOG_INFO("DIAG_BASE_HZ: override={:.0f} original={:.6g} applied={:.6g} "
                  "writer_hooks={} reader_hooks={} reader_value={:.6g}",
                  rex::cvar::Query<double>("engine_base_hz_override"), cur.base_hz_original,
                  cur.base_hz_applied, cur.base_hz_writer_hook_count,
                  cur.base_hz_reader_hook_count, cur.base_hz_reader_value);
      constexpr std::array<uint32_t, 8> kUiFixAddresses = {
          0x8243F470, 0x8245FE10, 0x82478238, 0x82496118,
          0x827B55D0, 0x827B2CD0, 0x827B2CF0, 0x827B51EC};
      std::ostringstream ui_fix;
      for (size_t i = 0; i < kUiFixAddresses.size(); ++i) {
        if (i != 0) ui_fix << " ";
        ui_fix << std::hex << std::uppercase << kUiFixAddresses[i] << std::dec
               << "=" << std::fixed << std::setprecision(1)
               << rate(cur.ui_fix_site_counts[i], prev.ui_fix_site_counts[i])
               << "/s:" << std::setprecision(6) << cur.ui_fix_site_original[i]
               << ">" << cur.ui_fix_site_applied[i];
      }
      REXLOG_INFO("DIAG_UI_FIX: enabled={} {}",
                  rex::cvar::Query<bool>("ui_timing_fix"), ui_fix.str());
      REXLOG_INFO("DIAG_CATCHING_SOULS: enabled={} hits={} rate={:.1f}/s "
                  "original={:.6g} scaled={:.6g} factor={:.6g}",
                  rex::cvar::Query<bool>("catching_souls_timing_fix"),
                  cur.catching_souls_hit_count,
                  rate(cur.catching_souls_hit_count, prev.catching_souls_hit_count),
                  cur.catching_souls_original_increment,
                  cur.catching_souls_scaled_increment, cur.catching_souls_factor);
      REXLOG_INFO("DIAG_ABSOLVE: update={:.1f}/s timer_calls={:.1f}/s "
                  "prompt_calls={:.1f}/s timer_orig={:.9g} timer_scaled={:.9g} "
                  "prompt_inc={:.1f}/s prompt_orig={:.9g} prompt_scaled={:.9g} "
                  "roll_step={:.1f}/s roll_orig={:.9g} roll_scaled={:.9g} "
                  "prompt_value={:.9g} threshold={:.9g}",
                  rate(cur.absolve_update_vtable_calls,
                       prev.absolve_update_vtable_calls),
                  rate(cur.absolve_timer_increment_calls,
                       prev.absolve_timer_increment_calls),
                  rate(cur.absolve_prompt_timer_calls,
                       prev.absolve_prompt_timer_calls),
                  cur.absolve_timer_last_original, cur.absolve_timer_last_scaled,
                  rate(cur.absolve_prompt_timer_increment_calls,
                       prev.absolve_prompt_timer_increment_calls),
                  cur.absolve_prompt_timer_last_original,
                  cur.absolve_prompt_timer_last_scaled,
                  rate(cur.absolve_roll_step_calls,
                       prev.absolve_roll_step_calls),
                  cur.absolve_roll_step_last_original,
                  cur.absolve_roll_step_last_scaled,
                  cur.absolve_prompt_timer_last_value,
                  cur.absolve_prompt_timer_last_threshold);
      prev = cur;
      prev_guest_frames = guest_frames;
      last_log = now;
    }
  }
};
