// dantes_inferno_native_app.h - Derived app with native renderer integration
//
// This is a COPY/DERIVATIVE of the original dantes_inferno_app.h.
// It inherits from DantesInfernoApp and adds native DiligentCore/Vulkan
// renderer support. The original src/dantes_inferno_app.h is NOT modified.
//
// Part of the DiligentCore migration (IMPL-DC-003).
// See: tools/research/dante_re/TRANSITION/DILIGENTCORE_MIGRATION_PLAN.md

#pragma once

#include "dantes_inferno_app.h"
#include "native_renderer/native_renderer_integration.h"

// Derived app class that adds native renderer support on top of the
// original DantesInfernoApp. All original behavior is inherited;
// native renderer hooks are added via overrides.
class DantesInfernoNativeApp : public DantesInfernoApp {
 public:
  using DantesInfernoApp::DantesInfernoApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<DantesInfernoNativeApp>(
        new DantesInfernoNativeApp(ctx, "dantes_inferno", PPCImageConfig));
  }

  void OnPostSetup() override {
    // Call base class first (sets up time scalar, FPS overlay, keybinds)
    DantesInfernoApp::OnPostSetup();

    // Initialize native renderer using the app's SDL window
    if (window()) {
      void* hwnd = window()->GetNativeWindowHandle();
      if (hwnd) {
        native_renderer_ = std::make_unique<dante::NativeRendererIntegration>();
        uint32_t w = window()->GetDesiredLogicalWidth();
        uint32_t h = window()->GetDesiredLogicalHeight();
        if (w == 0) w = 1280;
        if (h == 0) h = 720;
        if (!native_renderer_->initialize(hwnd, w, h)) {
          REXLOG_WARN("Native renderer init failed; staying on Xenos path");
          native_renderer_.reset();
        }
      }
    }
  }

  void OnWindowPixelSizeChanged(uint32_t pixel_width,
                                 uint32_t pixel_height) override {
    DantesInfernoApp::OnWindowPixelSizeChanged(pixel_width, pixel_height);
    if (native_renderer_) {
      native_renderer_->onResize(pixel_width, pixel_height);
    }
  }

  void OnShutdown() override {
    native_renderer_.reset();
    DantesInfernoApp::OnShutdown();
  }

 private:
  std::unique_ptr<dante::NativeRendererIntegration> native_renderer_;
};
