# Hybrid Renderer Migration Plan

## Goal

Incrementally replace the ReXGlue Xenos renderer with a project-owned
DiligentCore (Vulkan) renderer, one subsystem at a time. The game stays
fully playable at every step. ReXGlue remains as fallback.

## Principles

- The Xenos renderer is the base. We replace pieces, not the whole thing.
- Each step produces a working build. No big-bang rewrites.
- DiligentCore Vulkan is the target backend from step 1. No D3D12 detour.
- ReXGlue's ucode translator handles most shaders. We only reauthor
  shaders that need modification.
- ReXGlue stays as a runtime fallback via a cvar toggle.

## Steps

### Step 1: Presenter replacement

Replace ReXGlue's presenter with a DiligentCore Vulkan swapchain.

The Xenos command processor still renders the game into its own render
targets. We intercept at the present boundary and present through our
Vulkan swapchain instead of the SDK's D3D12 presenter.

Gains: resolution control, vsync control, insertion point for
post-processing, Vulkan surface from the existing SDL window.

Risk: low. Presenter is a clean boundary in the SDK.

### Step 2: Render target override

Create native Vulkan render targets at any resolution instead of Xenos
EDRAM 1280x720.

The Xenos command processor still processes draw calls, but draws land
in our native-size render targets. The Xenos EDRAM tiling emulation is
bypassed.

Gains: true 1440p/4K/ultrawide native rendering, not upscaled 720p.

Risk: medium. Requires understanding Xenos render target binding and
EDRAM layout. RE-030 has partial findings.

### Step 3: Post-processing injection

Insert TAA, FSR, FXAA between the game's render output and present.

We own the present path (Step 1) and the render targets (Step 2).
Motion vectors come from camera delta. The projection matrix address
is known (sub_8251DC68, confirmed during ultrawide work).

Gains: modern AA, upscaling, motion blur control, color grading.

Risk: medium. Motion vector generation requires camera delta tracking.

### Step 4: Texture cache replacement

Replace Xenos texture format emulation with native Vulkan texture
creation from game data.

Read TG4D/DXT textures from emulated memory, create native DiligentCore
textures. The asset_tool.py already understands TG4D/DXT formats.

Gains: no Xenos texture format overhead, native compressed formats,
direct control over texture streaming.

Risk: medium. Requires understanding runtime texture binding (U-DC-003,
currently PARTIAL).

### Step 5: Shader pipeline (hybrid)

Keep ReXGlue's ucode translator for most of the ~100 inferno_* shaders.
Reauthor specific shaders that need changes:

- Projection/aspect shader (for ultrawide without midasm hooks)
- Custom post-process shaders (TAA resolve, FSR, color grading)
- Any shader needing material parameter overrides

Gains: no shader stutter for reauthored shaders, control over specific
passes, ability to add new rendering techniques.

Risk: high for reauthored shaders. Low for kept shaders. The hybrid
approach limits risk to only the shaders we touch.

### Step 6: Command processor replacement

Replace the Xenos command processor with native Vulkan draw submission.

Read draw call data from emulated memory and submit directly through
DiligentCore/Vulkan. This is the point where the Xenos renderer is
fully retired for the replaced render paths.

Gains: full control over draw submission, no Xenos command buffer
overhead, ability to reorder or batch draws.

Risk: high. This is the most complex step. Requires complete
understanding of draw call data (U-DC-006, currently NOT STARTED).

### Step 7: Cross-platform validation

Build and test on Linux with Vulkan.

- Mesa RADV (AMD)
- ANV (Intel)
- NVIDIA proprietary

Verify all features work on both Windows and Linux:
- Native rendering
- Resolution/aspect/framerate control
- TAA/FSR
- Controller remapping
- Glyph replacement
- Exit game option

Gains: cross-platform, no Windows/D3D12 dependency.

Risk: low. Vulkan is cross-platform by design. Issues are likely
driver-specific.

## Current Status

| Step | Status |
|------|--------|
| Infrastructure (DiligentCore build, Vulkan device, swapchain, test draw) | DONE (IMPL-DC-001 through DC-003) |
| Step 1: Presenter replacement | DONE |
| Step 2: Render target override | NOT STARTED |
| Step 3: Post-processing injection | NOT STARTED |
| Step 4: Texture cache replacement | NOT STARTED |
| Step 5: Shader pipeline (hybrid) | NOT STARTED |
| Step 6: Command processor replacement | NOT STARTED |
| Step 7: Cross-platform validation | NOT STARTED |

## RE Dependencies

| Step | RE needed | Status |
|------|-----------|--------|
| Step 1 | None (infrastructure done) | READY |
| Step 2 | Xenos RT binding, EDRAM layout | PARTIAL (RE-030) |
| Step 3 | Camera delta (projection matrix known) | MOSTLY READY |
| Step 4 | Runtime texture binding (U-DC-003) | PARTIAL |
| Step 5 | Material parameters for reauthored shaders | PARTIAL (RE-030) |
| Step 6 | Draw call data (U-DC-006) | NOT STARTED |
| Step 7 | None | N/A |

## Relationship to Prior Work

- RE-001 through RE-031: architecture research (COMPLETE)
- IMPL-DC-001 through DC-003: DiligentCore infrastructure (COMPLETE)
- Ultrawide support: projection matrix hook at sub_8251DC68 (COMPLETE)
- Exit game keybind: Alt+F4 via RequestDeferredQuit (COMPLETE)
- Input remapping: SDL3 keybinds in OnPreSetup (COMPLETE)
- Version tracking fix: package-release.ps1 (COMPLETE)

## Step 1 Implementation Details

### How the ReXGlue presenter works

The ReXGlue presenter is a clean boundary between the Xenos GPU emulator
and the host display:

1. `ReXApp::SetupPresentation()` creates the graphics system (Xenos D3D12),
   which creates a `D3D12Provider` and `D3D12Presenter`.
2. The `D3D12Presenter` creates a DXGI swapchain on the window's HWND.
3. The Xenos D3D12 command processor renders the game into D3D12 textures.
4. On swap, `CommandProcessor::IssueSwap()` calls
   `presenter->RefreshGuestOutput(width, height, aspect, refresher_callback)`.
5. The refresher callback writes the rendered output into a D3D12 "guest
   output" texture (R10G10B10A2_UNORM, surface-independent).
6. The presenter stores this in a triple-buffered mailbox.
7. On the UI thread, `Window::OnPaint()` calls
   `presenter->PaintFromUIThread()` which composites the guest output onto
   the DXGI swapchain (with scaling, FXAA, gamma, letterboxing) and presents.

Key APIs at the boundary:
- `Presenter::RefreshGuestOutput()` — called by the command processor (GPU thread)
- `Presenter::CaptureGuestOutput(RawImage&)` — reads back guest output as
  CPU-side RGBA8 (R8G8B8X8, 8bpc). Callable from any thread.
- `Presenter::guest_frame_count()` — atomic counter, incremented on each
  RefreshGuestOutput. Used to detect new frames.
- `Window::SetPresenter(nullptr)` — disconnects the presenter from the
  window, destroying its DXGI swapchain. The presenter still receives
  RefreshGuestOutput but no longer presents.

### What was implemented

**Approach:** CPU readback via `CaptureGuestOutput()` + Vulkan upload + present.

The D3D12 presenter is kept alive (it still receives `RefreshGuestOutput`
from the Xenos command processor) but disconnected from the window so it
stops presenting via DXGI. A DiligentCore Vulkan swapchain is created on
the same HWND. A background thread polls `guest_frame_count()` for new
frames, calls `CaptureGuestOutput()` to get a CPU-side RGBA8 image, and
passes it to `NativeDevice::presentImage()` which uploads it to a Vulkan
texture, blits it to the swapchain via a fullscreen-triangle pipeline,
and presents.

The copy through CPU memory is a first-pass approach. Future steps will
use GPU-GPU interop (VK_NV_external_memory_win32 or shared handle) to
avoid the CPU round-trip.

### Files created

| File | Purpose |
|------|---------|
| `src/native_renderer/native_presenter.h` | NativePresenter class interface |
| `src/native_renderer/native_presenter.cpp` | Presenter replacement implementation (background thread, CaptureGuestOutput polling, Vulkan present) |

### Files modified

| File | Change |
|------|--------|
| `src/native_renderer/native_device.h` | Added `presentImage()` method |
| `src/native_renderer/native_device.cpp` | Added blit pipeline (fullscreen triangle VS+PS), staging texture, `presentImage()` implementation, blit resource cleanup in `shutdown()` |
| `src/dantes_inferno_app.h` | Conditional native presenter integration in `OnPostSetup`/`OnShutdown` (gated by `DANTESINFERNO_NATIVE_RENDERER` preprocessor + `use_native_presenter` cvar) |
| `CMakeLists.txt` | Added `native_presenter.cpp` to `dante_native_renderer`, linked `dante_native_renderer` to `dantes_inferno` target, added `DANTESINFERNO_NATIVE_RENDERER` preprocessor definition |

### Cvar

- `use_native_presenter` (bool, default: false, category: "Graphics") —
  When true, replaces the D3D12 presenter with the Vulkan presenter.
  When false (or when `DANTESINFERNO_NATIVE_RENDERER=OFF`), the D3D12
  presenter is used as before.

### Build verification

- `DANTESINFERNO_NATIVE_RENDERER=OFF`: builds and links successfully
  (866 objects, 41MB executable). No native renderer code linked.
- `DANTESINFERNO_NATIVE_RENDERER=ON`: builds and links successfully
  (1325 objects, 52MB executable). Native presenter linked into
  `dantes_inferno.exe`. `native_renderer_test.exe` also builds.

### Known limitations

1. **CPU readback copy path** — `CaptureGuestOutput()` does a D3D12 GPU
   readback (create readback buffer, copy, map). This adds latency and
   bandwidth overhead. Future optimization: VK_NV_external_memory_win32
   or D3D12 shared handle for GPU-GPU texture sharing.

2. **No UI overlay support** — When the native presenter is active, ImGui
   overlays (FPS overlay, debug console, settings) are not rendered. The
   D3D12 presenter's `PaintFromUIThread` path handles UI compositing;
   since it's disconnected, UI is not drawn. Future work: render ImGui
   through the Vulkan path or composite UI separately.

3. **No scaling/letterboxing** — The blit pipeline uses linear sampling
   to stretch the guest output to fill the swapchain. No letterboxing,
   FXAA, gamma ramp, or FSR is applied. These were handled by the D3D12
   presenter's paint pipeline. Future Step 3 will add post-processing.

4. **Background thread present** — The present loop runs on a background
   thread, not the UI thread. This is simpler but may have timing issues
   with window resize events. The resize is forwarded to the Vulkan
   swapchain but the D3D12 presenter's resize path is bypassed.

5. **HLSL shader compilation** — The blit shaders use HLSL compiled to
   SPIR-V by DiligentCore. `DILIGENT_NO_HLSL=ON` is set in CMake, but
   the Vulkan backend's glslang HLSL front-end is still available.
   If HLSL support is fully removed in a future DiligentCore update,
   the blit shaders should be converted to GLSL.

### What the next step should address

Step 2 (render target override) should:
- Replace the CPU readback copy with GPU-GPU texture sharing
  (VK_NV_external_memory_win32 or D3D12 shared handle exported to
  Vulkan). This eliminates the CPU round-trip and is the prerequisite
  for native resolution rendering.
- Or, alternatively, create native Vulkan render targets at the desired
  resolution and have the Xenos command processor render into them
  directly, bypassing the D3D12 render target cache entirely.
