
#pragma once

#include "dantes_inferno_app.h"
#include "native_renderer/dante_graphics_system.h"
#include "native_renderer/native_presenter.h"

#include <rex/cvar.h>
#include <rex/graphics/graphics_system.h>
#include <rex/runtime.h>

class DantesInfernoNativeApp : public DantesInfernoApp {
 public:
  using DantesInfernoApp::DantesInfernoApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<DantesInfernoNativeApp>(
        new DantesInfernoNativeApp(ctx, "dantes_inferno", PPCImageConfig));
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    DantesInfernoApp::OnPreSetup(config);
    auto graphics = dante::CreateConfiguredGraphicsSystem(config.gpu_plugin);
    if (graphics) {
      config.graphics = std::move(graphics);
    }
  }

  void OnPostSetup() override {
    DantesInfernoApp::OnPostSetup();

    if (!rex::cvar::Query<bool>("use_native_presenter")) return;

    // The in-process Vulkan backend presents through its own VulkanPresenter;
    // layering NativePresenter on top would disconnect it, spin up a second
    // (DiligentCore) device, and fall back to CPU readback.
    if (rex::cvar::Query<std::string>("renderer") == "native") {
      REXLOG_INFO(
          "use_native_presenter ignored: in-process Vulkan backend presents "
          "directly");
      return;
    }

    if (!window()) return;

    auto* gfx_sys = runtime() ? runtime()->graphics_system() : nullptr;
    auto* presenter = gfx_sys ? gfx_sys->presenter() : nullptr;
    if (!presenter) {
      REXLOG_WARN("Native presenter: no graphics presenter; staying on Xenos path");
      return;
    }

    native_presenter_ = std::make_unique<dante::NativePresenter>();
    uint32_t w = window()->GetDesiredLogicalWidth();
    uint32_t h = window()->GetDesiredLogicalHeight();
    if (w == 0) w = 1280;
    if (h == 0) h = 720;
    if (!native_presenter_->initialize(presenter, window(), w, h)) {
      REXLOG_WARN("Native presenter init failed; staying on Xenos path");
      native_presenter_.reset();
    }
  }

  void OnShutdown() override {
    native_presenter_.reset();
    DantesInfernoApp::OnShutdown();
  }

 private:
  std::unique_ptr<dante::NativePresenter> native_presenter_;
};
