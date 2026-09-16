#include "dante_graphics_system.h"

#include <rex/cvar.h>
#include <rex/logging/macros.h>
#include <rex/system/gpu_plugin.h>

REXCVAR_DEFINE_STRING(renderer, "xenos", "Graphics",
                      "Graphics system: 'xenos' loads the rexgpu-xenos plugin "
                      "(default), 'native' routes through DanteGraphicsSystem");

namespace dante {

DanteGraphicsSystem::DanteGraphicsSystem(
    std::unique_ptr<rex::system::IGraphicsSystem> inner)
    : inner_(std::move(inner)) {
  REXLOG_INFO("DanteGraphicsSystem: created");
}

rex::X_STATUS DanteGraphicsSystem::SetupPresentation(
    rex::ui::WindowedAppContext* app_context) {
  REXLOG_INFO("DanteGraphicsSystem: SetupPresentation");
  return inner_->SetupPresentation(app_context);
}

rex::X_STATUS DanteGraphicsSystem::SetupGuestGpu(
    rex::runtime::FunctionDispatcher* function_dispatcher,
    rex::system::KernelState* kernel_state) {
  REXLOG_INFO("DanteGraphicsSystem: SetupGuestGpu");
  return inner_->SetupGuestGpu(function_dispatcher, kernel_state);
}

bool DanteGraphicsSystem::has_presentation() const {
  return inner_->has_presentation();
}

rex::ui::GraphicsProvider* DanteGraphicsSystem::provider() const {
  return inner_->provider();
}

rex::ui::Presenter* DanteGraphicsSystem::presenter() const {
  return inner_->presenter();
}

void DanteGraphicsSystem::SetInterruptCallback(uint32_t callback,
                                               uint32_t user_data) {
  inner_->SetInterruptCallback(callback, user_data);
}

void DanteGraphicsSystem::InitializeRingBuffer(uint32_t ptr,
                                               uint32_t size_log2) {
  REXLOG_INFO(
      "DanteGraphicsSystem: InitializeRingBuffer ptr={:#x} size_log2={}", ptr,
      size_log2);
  inner_->InitializeRingBuffer(ptr, size_log2);
}

void DanteGraphicsSystem::EnableReadPointerWriteBack(
    uint32_t ptr, uint32_t block_size_log2) {
  inner_->EnableReadPointerWriteBack(ptr, block_size_log2);
}

void DanteGraphicsSystem::InitializeShaderStorage(
    const std::filesystem::path& cache_root, uint32_t title_id, bool blocking) {
  inner_->InitializeShaderStorage(cache_root, title_id, blocking);
}

void DanteGraphicsSystem::Shutdown() {
  REXLOG_INFO("DanteGraphicsSystem: Shutdown");
  inner_->Shutdown();
}

std::unique_ptr<rex::system::IGraphicsSystem> CreateConfiguredGraphicsSystem(
    const std::string& gpu_plugin) {
  const std::string renderer = rex::cvar::Query<std::string>("renderer");
  if (renderer == "xenos" || renderer == "rexglue") {
    return nullptr;
  }
  if (renderer == "native") {
    auto inner = rex::system::LoadGpuPlugin(gpu_plugin);
    if (!inner) {
      REXLOG_ERROR(
          "DanteGraphicsSystem: failed to load inner plugin '{}'; falling back "
          "to the default plugin path",
          gpu_plugin);
      return nullptr;
    }
    return std::make_unique<DanteGraphicsSystem>(std::move(inner));
  }
  REXLOG_WARN("Unknown renderer='{}'; using default xenos plugin", renderer);
  return nullptr;
}

}  // namespace dante
