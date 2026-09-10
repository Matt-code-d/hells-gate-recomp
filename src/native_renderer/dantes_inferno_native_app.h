








#pragma once

#include "dantes_inferno_app.h"
#include "native_renderer/native_renderer_integration.h"




class DantesInfernoNativeApp : public DantesInfernoApp {
 public:
  using DantesInfernoApp::DantesInfernoApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<DantesInfernoNativeApp>(
        new DantesInfernoNativeApp(ctx, "dantes_inferno", PPCImageConfig));
  }

  void OnPostSetup() override {
    
    DantesInfernoApp::OnPostSetup();

    
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
