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

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}
