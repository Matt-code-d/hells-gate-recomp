#include "native_presenter.h"
#include "native_device.h"

#include <rex/ui/presenter.h>
#include <rex/ui/window.h>
#include <rex/cvar.h>
#include <rex/logging/macros.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

namespace dante {

REXCVAR_DEFINE_BOOL(use_native_presenter, false, "Graphics",
                    "Use native DiligentCore/Vulkan presenter instead of "
                    "ReXGlue's D3D12 presenter (Step 1 hybrid renderer)");

REXCVAR_DEFINE_BOOL(use_gpu_interop, false, "Graphics",
                    "Request experimental D3D12->Vulkan texture sharing; "
                    "falls back to CPU while external ownership is unverified");

static void computeLetterbox(NativeDevice& device, uint32_t display_x,
                             uint32_t display_y) {
  const double configured_aspect =
      rex::cvar::Query<double>("ultrawide_target_aspect");
  double aspect = configured_aspect;
  if (!(aspect > 0.0)) {
    aspect = display_x && display_y ? double(display_x) / display_y :
        double(rex::cvar::Query<int32_t>("video_mode_width")) /
        rex::cvar::Query<int32_t>("video_mode_height");
  }
  device.setDisplayAspect(aspect, rex::cvar::Query<bool>("present_letterbox"));
}

struct NativePresenter::Impl {
  NativeDevice device;
  rex::ui::Presenter* rex_presenter = nullptr;
  rex::ui::Window* window = nullptr;
  bool presenter_disconnected = false;
  std::atomic<bool> running{false};
  std::atomic<bool> stop_requested{false};
  std::thread present_thread;
  uint64_t last_frame_count = 0;
  bool gpu_interop_active = false;
  bool gpu_interop_failed = false;

  std::atomic<uint64_t> diag_capture_ok{0};
  std::atomic<uint64_t> diag_capture_fail{0};
  std::atomic<uint64_t> diag_empty_image{0};
  std::atomic<uint64_t> diag_interop_ok{0};
  std::atomic<uint64_t> diag_interop_fail{0};

  ~Impl() {
    stop();
  }

  void stop() {
    stop_requested.store(true);
    if (present_thread.joinable())
      present_thread.join();
    running.store(false);
  }

  void presentLoop() {
    REXLOG_INFO("NativePresenter: present thread started; guest_vsync={} "
                "host_vsync={} time_scalar={} video_mode_hz={} resolution_scale={} "
                "swap_post_effect={} use_gpu_interop={}",
                rex::cvar::Query<bool>("vsync"),
                rex::cvar::Query<bool>("d3d12_host_vsync"),
                rex::cvar::Query<double>("time_scalar"),
                rex::cvar::Query<double>("video_mode_refresh_rate"),
                rex::cvar::Query<int32_t>("resolution_scale"),
                rex::cvar::Query<std::string>("swap_post_effect"),
                rex::cvar::Query<bool>("use_gpu_interop"));

    gpu_interop_active = rex::cvar::Query<bool>("use_gpu_interop");
    gpu_interop_failed = false;
    if (gpu_interop_active) {
      REXLOG_INFO("NativePresenter: GPU interop requested; will fall back to "
                  "CPU readback if initialization fails at runtime");
    }

    constexpr int kMaxStartupWaitMs = 10000;
    auto start = std::chrono::steady_clock::now();
    while (!stop_requested.load()) {
      uint64_t fc = rex_presenter->guest_frame_count();
      if (fc > 0) {
        REXLOG_INFO("NativePresenter: first guest frame detected (count={})", fc);
        break;
      }
      auto elapsed = std::chrono::steady_clock::now() - start;
      if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()
          > kMaxStartupWaitMs) {
        REXLOG_WARN("NativePresenter: no guest frames after {}ms, "
                    "presenting clear", kMaxStartupWaitMs);
        device.beginFrame();
        device.clear(0.0f, 0.0f, 0.0f, 1.0f);
        device.present(rex::cvar::Query<bool>("d3d12_host_vsync") ? 1 : 0);
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    uint64_t capture_ok_count = 0;
    uint64_t capture_fail_count = 0;
    uint64_t empty_image_count = 0;
    uint64_t interop_ok_count = 0;
    uint64_t interop_fail_count = 0;
    uint64_t log_counter = 0;
    using Clock = std::chrono::steady_clock;
    auto timing_start = Clock::now();
    uint64_t timing_guest_start = rex_presenter->guest_frame_count();
    uint64_t timing_captures = 0;
    uint64_t timing_attempts = 0;
    uint64_t timing_presented = 0;
    uint32_t timing_windows = 0;
    uint32_t last_sync_interval = UINT32_MAX;
    double capture_cpu_ms = 0.0;
    double upload_draw_cpu_ms = 0.0;
    double present_cpu_ms = 0.0;

    while (!stop_requested.load()) {
      if (!device.updateWindowSize()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        continue;
      }
      uint64_t current_count = rex_presenter->guest_frame_count();
      const uint32_t sync_interval =
          rex::cvar::Query<bool>("d3d12_host_vsync") ? 1 : 0;
      if (sync_interval != last_sync_interval) {
        REXLOG_INFO("NativePresenter: host_sync_interval={} guest_vsync={} time_scalar={}",
                    sync_interval, rex::cvar::Query<bool>("vsync"),
                    rex::cvar::Query<double>("time_scalar"));
        last_sync_interval = sync_interval;
      }
      const bool measure = timing_windows < 3;

      if (current_count != last_frame_count) {
        last_frame_count = current_count;

        if (gpu_interop_active && !gpu_interop_failed) {
          rex::ui::Presenter::GuestOutputSharedTexture shared;
          const auto capture_start = measure ? Clock::now() : Clock::time_point{};
          const bool got_shared = rex_presenter->GetGuestOutputSharedTexture(shared);
          if (measure) {
            capture_cpu_ms += std::chrono::duration<double, std::milli>(
                Clock::now() - capture_start).count();
            ++timing_attempts;
          }
          if (got_shared && shared.handle && shared.width > 0 && shared.height > 0) {
            computeLetterbox(device, 0, 0);
            NativePresentTimings timings;
            if (measure) ++timing_captures;
            if (!device.presentImageShared(shared.handle, shared.width,
                                           shared.height, sync_interval,
                                           measure ? &timings : nullptr,
                                           shared.adapter_luid_low,
                                           shared.adapter_luid_high)) {
              interop_fail_count++;
              diag_interop_fail.fetch_add(1, std::memory_order_relaxed);
              if (interop_fail_count <= 3) {
                REXLOG_WARN("NativePresenter: presentImageShared failed (count={}), "
                            "falling back to CPU readback",
                            interop_fail_count);
              }
              if (interop_fail_count >= 3) {
                REXLOG_WARN("NativePresenter: GPU interop failed {} times, "
                            "disabling and falling back to CPU readback",
                            interop_fail_count);
                gpu_interop_failed = true;
                rex_presenter->ReleaseGuestOutputSharedTexture();
              }
            } else {
              interop_ok_count++;
              diag_interop_ok.fetch_add(1, std::memory_order_relaxed);
              rex_presenter->ReleaseGuestOutputSharedTexture();
              if (measure) {
                ++timing_presented;
                upload_draw_cpu_ms += timings.upload_draw_cpu_ms;
                present_cpu_ms += timings.present_cpu_ms;
              }
              if (interop_ok_count == 1 || interop_ok_count == 60 ||
                  interop_ok_count == 300) {
                REXLOG_INFO("NativePresenter: interop={} shared={}x{} "
                            "handle={:#x}",
                            interop_ok_count, shared.width, shared.height,
                            reinterpret_cast<uintptr_t>(shared.handle));
              }
            }
          } else {
            interop_fail_count++;
            diag_interop_fail.fetch_add(1, std::memory_order_relaxed);
            if (interop_fail_count <= 3) {
              REXLOG_WARN("NativePresenter: GetGuestOutputSharedTexture failed "
                          "(count={}), trying CPU readback",
                          interop_fail_count);
            }
            if (interop_fail_count >= 3) {
              gpu_interop_failed = true;
            }
          }
        }

        if (!gpu_interop_active || gpu_interop_failed) {
          rex::ui::RawImage image;
          const auto capture_start = measure ? Clock::now() : Clock::time_point{};
          const bool captured = rex_presenter->CaptureGuestOutput(image);
          if (measure) {
            capture_cpu_ms += std::chrono::duration<double, std::milli>(
                Clock::now() - capture_start).count();
            ++timing_attempts;
          }
          if (captured) {
            if (image.width > 0 && image.height > 0 &&
                image.stride >= size_t(image.width) * 4 &&
                image.data.size() >= size_t(image.width) * 4 &&
                image.height - 1 <=
                    (image.data.size() - size_t(image.width) * 4) / image.stride) {
              capture_ok_count++;
              diag_capture_ok.fetch_add(1, std::memory_order_relaxed);
              if (capture_ok_count == 1 || capture_ok_count == 60 ||
                  capture_ok_count == 300) {
                size_t nonblack_pixels = 0;
                for (uint32_t y = 0; y < image.height; ++y) {
                  const auto* row = image.data.data() + y * image.stride;
                  for (uint32_t x = 0; x < image.width; ++x) {
                    if (row[x * 4] || row[x * 4 + 1] || row[x * 4 + 2])
                      ++nonblack_pixels;
                  }
                }
                REXLOG_INFO("NativePresenter: capture={} image={}x{} stride={} "
                            "bytes={} nonblack_rgb_pixels={}",
                            capture_ok_count, image.width, image.height,
                            image.stride, image.data.size(), nonblack_pixels);
              }
              computeLetterbox(device, image.display_aspect_ratio_x,
                                image.display_aspect_ratio_y);
              NativePresentTimings timings;
              if (measure) ++timing_captures;
              if (!device.presentImage(image.width, image.height, image.data.data(),
                                       image.stride, sync_interval,
                                       measure ? &timings : nullptr)) {
                REXLOG_ERROR("NativePresenter: blit failed; stopping native presentation");
                break;
              }
              if (measure) {
                ++timing_presented;
                upload_draw_cpu_ms += timings.upload_draw_cpu_ms;
                present_cpu_ms += timings.present_cpu_ms;
              }
            } else {
              empty_image_count++;
              diag_empty_image.fetch_add(1, std::memory_order_relaxed);
              if (empty_image_count == 1) {
                REXLOG_WARN("NativePresenter: CaptureGuestOutput returned an invalid "
                            "image ({}x{}, stride={}, data_size={})",
                            image.width, image.height, image.stride, image.data.size());
              }
            }
          } else {
            capture_fail_count++;
            diag_capture_fail.fetch_add(1, std::memory_order_relaxed);
            device.beginFrame();
            device.clear(0.0f, 0.0f, 0.0f, 1.0f);
            device.present(rex::cvar::Query<bool>("d3d12_host_vsync") ? 1 : 0);
          }
        }

        if (log_counter == 0 || log_counter == 59 || log_counter == 299) {
          REXLOG_INFO("NativePresenter: frame={} interop_ok={} interop_fail={} "
                      "capture_ok={} capture_fail={} empty_img={} mode={}",
                      current_count, interop_ok_count, interop_fail_count,
                      capture_ok_count, capture_fail_count, empty_image_count,
                      (gpu_interop_active && !gpu_interop_failed) ? "gpu" : "cpu");
        }
        log_counter++;
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      if (measure) {
        const auto now = Clock::now();
        const double seconds = std::chrono::duration<double>(now - timing_start).count();
        if (seconds >= 5.0) {
          const uint64_t guest_now = rex_presenter->guest_frame_count();
          const uint64_t guest_delta = guest_now - timing_guest_start;
          const uint64_t skipped = guest_delta > timing_captures
              ? guest_delta - timing_captures : 0;
          REXLOG_INFO("NativePresenter: timing window={} seconds={:.3f} "
                      "guest_fps={:.2f} capture_fps={:.2f} submit_fps={:.2f} "
                      "guest_not_captured={} capture_cpu_ms={:.3f} "
                      "upload_draw_cpu_ms={:.3f} present_cpu_ms={:.3f} "
                      "host_sync_interval={} guest_vsync={} time_scalar={} "
                      "mode={}",
                      timing_windows + 1, seconds, guest_delta / seconds,
                      timing_captures / seconds, timing_presented / seconds, skipped,
                      timing_attempts ? capture_cpu_ms / timing_attempts : 0.0,
                      timing_presented ? upload_draw_cpu_ms / timing_presented : 0.0,
                      timing_presented ? present_cpu_ms / timing_presented : 0.0,
                      sync_interval, rex::cvar::Query<bool>("vsync"),
                      rex::cvar::Query<double>("time_scalar"),
                      (gpu_interop_active && !gpu_interop_failed) ? "gpu" : "cpu");
          ++timing_windows;
          timing_start = now;
          timing_guest_start = guest_now;
          timing_attempts = timing_captures = timing_presented = 0;
          capture_cpu_ms = upload_draw_cpu_ms = present_cpu_ms = 0.0;
        }
      }
    }

    running.store(false);
    REXLOG_INFO("NativePresenter: present thread stopped "
                "(interop_ok={} interop_fail={} ok={} fail={} empty={})",
                interop_ok_count, interop_fail_count,
                capture_ok_count, capture_fail_count, empty_image_count);
  }
};

NativePresenter::NativePresenter()
    : impl_(std::make_unique<Impl>()) {}

NativePresenter::~NativePresenter() {
  shutdown();
}

bool NativePresenter::initialize(rex::ui::Presenter* rex_presenter,
                                 rex::ui::Window* window,
                                 uint32_t width, uint32_t height) {
  shutdown();
  if (!rex_presenter || !window) {
    REXLOG_ERROR("NativePresenter: null presenter or window");
    return false;
  }

  if (width == 0) width = 1280;
  if (height == 0) height = 720;

  impl_->rex_presenter = rex_presenter;
  impl_->window = window;

  void* hwnd = window->GetNativeWindowHandle();
  if (!hwnd) {
    REXLOG_ERROR("NativePresenter: window has no native handle");
    shutdown();
    return false;
  }

  window->SetPresenter(nullptr);
  impl_->presenter_disconnected = true;
  REXLOG_INFO("NativePresenter: D3D12 presenter disconnected from window");

  if (!impl_->device.initialize(hwnd, width, height)) {
    REXLOG_ERROR("NativePresenter: failed to initialize native device");
    shutdown();
    return false;
  }

  impl_->last_frame_count = 0;
  impl_->stop_requested.store(false);
  impl_->running.store(true);
  impl_->present_thread = std::thread(&Impl::presentLoop, impl_.get());

  REXLOG_INFO("NativePresenter: initialized ({}x{})", width, height);
  return true;
}

void NativePresenter::shutdown() {
  if (!impl_->rex_presenter) return;

  impl_->stop();

  impl_->device.shutdown();

  if (impl_->presenter_disconnected && impl_->window) {
    impl_->window->SetPresenter(impl_->rex_presenter);
    impl_->presenter_disconnected = false;
    REXLOG_INFO("NativePresenter: D3D12 presenter reconnected to window");
  }

  impl_->rex_presenter = nullptr;
  impl_->window = nullptr;
}

bool NativePresenter::isInitialized() const {
  return impl_ && impl_->running.load();
}

NativePresenterMetrics NativePresenter::GetMetrics() const {
  NativePresenterMetrics out{};
  if (!impl_ || !impl_->rex_presenter) return out;
  out.guest_frame_count = impl_->rex_presenter->guest_frame_count();
  out.capture_ok = impl_->diag_capture_ok.load(std::memory_order_relaxed);
  out.capture_fail = impl_->diag_capture_fail.load(std::memory_order_relaxed);
  out.empty_image = impl_->diag_empty_image.load(std::memory_order_relaxed);
  out.interop_ok = impl_->diag_interop_ok.load(std::memory_order_relaxed);
  out.interop_fail = impl_->diag_interop_fail.load(std::memory_order_relaxed);
  out.gpu_interop_active = impl_->gpu_interop_active;
  out.gpu_interop_failed = impl_->gpu_interop_failed;
  return out;
}

}
