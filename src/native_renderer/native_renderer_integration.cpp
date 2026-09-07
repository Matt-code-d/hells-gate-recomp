// native_renderer_integration.cpp - Integration with ReXGlue app
//
// Bridges the project-owned native renderer with the ReXGlue SDL window.
// Gated behind the `native_renderer` cvar.
//
// Part of the DiligentCore migration (IMPL-DC-003).

#include "native_renderer_integration.h"
#include "native_device.h"

#include <rex/cvar.h>
#include <rex/logging/macros.h>

#include <cmath>

namespace dante {

REXCVAR_DEFINE_BOOL(native_renderer, false, "Graphics",
                    "Use native DiligentCore/Vulkan renderer instead of ReXGlue Xenos");

struct NativeRendererIntegration::Impl {
  NativeDevice device;
  bool initialized = false;
  float frame_time = 0.0f;
};

NativeRendererIntegration::NativeRendererIntegration()
    : impl_(std::make_unique<Impl>()) {}

NativeRendererIntegration::~NativeRendererIntegration() {
  shutdown();
}

bool NativeRendererIntegration::initialize(void* hwnd, uint32_t width,
                                            uint32_t height) {
  if (impl_->initialized) return true;

  if (!impl_->device.initialize(hwnd, width, height)) {
    REXLOG_ERROR("NativeRendererIntegration: Failed to initialize native device");
    return false;
  }

  impl_->initialized = true;
  REXLOG_INFO("NativeRendererIntegration: Initialized ({}x{})", width, height);
  return true;
}

void NativeRendererIntegration::onFrame() {
  if (!impl_->initialized) return;
  if (!REXCVAR_GET(native_renderer)) return;

  impl_->frame_time += 0.02f;
  float r = 0.1f + 0.05f * sinf(impl_->frame_time);
  float g = 0.1f + 0.05f * sinf(impl_->frame_time + 2.094f);
  float b = 0.15f + 0.05f * sinf(impl_->frame_time + 4.189f);

  impl_->device.beginFrame();
  impl_->device.clear(r, g, b, 1.0f);
  impl_->device.present(1);
}

void NativeRendererIntegration::onResize(uint32_t width, uint32_t height) {
  if (!impl_->initialized) return;
  impl_->device.resize(width, height);
}

void NativeRendererIntegration::shutdown() {
  if (!impl_->initialized) return;
  impl_->device.shutdown();
  impl_->initialized = false;
}

bool NativeRendererIntegration::isInitialized() const {
  return impl_ && impl_->initialized;
}

bool NativeRendererIntegration::isActive() const {
  return isInitialized() && REXCVAR_GET(native_renderer);
}

}  // namespace dante
