// native_renderer_integration.h - Integration of native renderer with ReXGlue app
//
// This module bridges the project-owned native renderer (DiligentCore/Vulkan)
// with the ReXGlue application's SDL window. It is gated behind a cvar and
// does not interfere with the existing ReXGlue Xenos renderer unless enabled.
//
// Part of the DiligentCore migration (IMPL-DC-003).
// See: tools/research/dante_re/TRANSITION/DILIGENTCORE_MIGRATION_PLAN.md

#pragma once

#include <memory>

namespace dante {

// Integration controller for the native renderer within the ReXGlue app.
// Created in OnPostSetup, destroyed in OnShutdown.
class NativeRendererIntegration {
 public:
  NativeRendererIntegration();
  ~NativeRendererIntegration();

  // Initialize the native renderer using the app's window handle.
  // hwnd: the ReXGlue SDL window's native HWND.
  // width/height: current window pixel dimensions.
  // Returns true on success.
  bool initialize(void* hwnd, uint32_t width, uint32_t height);

  // Called every frame from the UI thread. When the native renderer is
  // enabled (via cvar), it renders a clear color to the window.
  // When disabled, this is a no-op.
  void onFrame();

  // Called when the window is resized.
  void onResize(uint32_t width, uint32_t height);

  // Shut down and release all resources.
  void shutdown();

  // Check if the native renderer is initialized.
  bool isInitialized() const;

  // Check if the native renderer is currently active (enabled via cvar).
  bool isActive() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace dante
