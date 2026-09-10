#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

namespace rex {
namespace ui {
class Presenter;
class Window;
}
}

namespace dante {

struct NativePresenterMetrics {
  uint64_t guest_frame_count = 0;
  uint64_t capture_ok = 0;
  uint64_t capture_fail = 0;
  uint64_t empty_image = 0;
  uint64_t interop_ok = 0;
  uint64_t interop_fail = 0;
  bool gpu_interop_active = false;
  bool gpu_interop_failed = false;
};

class NativePresenter {
 public:
  NativePresenter();
  ~NativePresenter();

  NativePresenter(const NativePresenter&) = delete;
  NativePresenter& operator=(const NativePresenter&) = delete;

  bool initialize(rex::ui::Presenter* rex_presenter, rex::ui::Window* window,
                  uint32_t width, uint32_t height);

  void shutdown();

  bool isInitialized() const;

  NativePresenterMetrics GetMetrics() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}
