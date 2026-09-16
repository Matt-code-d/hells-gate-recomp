#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>

#include "native_device.h"

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                   LPARAM lParam) {
  switch (msg) {
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    case WM_KEYDOWN:
      if (wParam == VK_ESCAPE) {
        PostQuitMessage(0);
      }
      return 0;
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

struct VertexTri {
  float pos[2];
  float color[3];
};

struct VertexQuad {
  float pos[2];
  float uv[2];
};

struct CameraCB {
  float transform[16];
};

static const char* kVSTri = R"(
#version 450
layout(binding = 0) uniform CameraCB { mat4 g_Transform; };
layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec3 in_color;
layout(location = 0) out vec3 out_color;
void main() {
    gl_Position = g_Transform * vec4(in_pos, 0.0, 1.0);
    out_color = in_color;
}
)";

static const char* kPSTri = R"(
#version 450
layout(location = 0) in vec3 in_color;
layout(location = 0) out vec4 out_color;
void main() {
    out_color = vec4(in_color, 1.0);
}
)";

static const char* kVSQuad = R"(
#version 450
layout(binding = 0) uniform CameraCB { mat4 g_Transform; };
layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 0) out vec2 out_uv;
void main() {
    gl_Position = g_Transform * vec4(in_pos, 0.0, 1.0);
    out_uv = in_uv;
}
)";

static const char* kPSQuad = R"(
#version 450
layout(binding = 1) uniform texture2D g_Texture;
layout(binding = 2) uniform sampler g_Sampler;
layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 out_color;
void main() {
    out_color = texture(sampler2D(g_Texture, g_Sampler), in_uv);
}
)";

static void buildTransformMatrix(float angle, float scale, float tx, float ty,
                                 float* out) {
  float c = cosf(angle) * scale;
  float s = sinf(angle) * scale;
  out[0] = c;  out[1] = s;  out[2] = 0; out[3] = 0;
  out[4] = -s; out[5] = c;  out[6] = 0; out[7] = 0;
  out[8] = 0;  out[9] = 0;  out[10] = 1; out[11] = 0;
  out[12] = tx; out[13] = ty; out[14] = 0; out[15] = 1;
}

static void genCheckerboard(uint32_t* pixels, uint32_t w, uint32_t h) {
  for (uint32_t y = 0; y < h; y++) {
    for (uint32_t x = 0; x < w; x++) {
      bool checker = ((x / 8) + (y / 8)) % 2 == 0;
      if (checker) {
        pixels[y * w + x] = 0xFF'FF'FF'FF;
      } else {
        uint8_t r = static_cast<uint8_t>(x * 4);
        uint8_t g = static_cast<uint8_t>(y * 4);
        pixels[y * w + x] = 0xFF000000u | (r) | (g << 8);
      }
    }
  }
}

static bool runBlitTest(dante::NativeDevice& device, HWND hwnd) {
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  for (uint32_t phase = 0; phase < 3; ++phase) {
    const uint32_t w = phase == 1 ? 97 : 64;
    const uint32_t h = phase == 1 ? 53 : 48;
    const size_t stride = w * 4 + 28;
    const uint32_t colors[3][4] = {
        {0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF},
        {0xFF808080, 0xFFC08040, 0xFF204060, 0xFF101010},
        {0xFF404040, 0xFF4080C0, 0xFF604020, 0xFFB0B0B0}};
    std::vector<uint8_t> pixels(stride * h, 0x7F);
    for (uint32_t y = 0; y < h; ++y) {
      for (uint32_t x = 0; x < w; ++x) {
        const uint32_t color = colors[phase][(y >= h / 2) * 2 + (x >= w / 2)];
        std::memcpy(pixels.data() + y * stride + x * 4, &color, 4);
      }
    }
    for (uint32_t frame = 0; frame < 60; ++frame) {
      MSG msg;
      while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return false;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
      }
      if (!device.presentImage(w, h, pixels.data(), stride, 1)) return false;
    }
    Sleep(100);
    RECT client;
    GetClientRect(hwnd, &client);
    HDC dc = GetDC(nullptr);
    bool passed = true;
    for (uint32_t quadrant = 0; quadrant < 4; ++quadrant) {
      POINT point{client.right * (quadrant % 2 ? 3 : 1) / 4,
                  client.bottom * (quadrant / 2 ? 3 : 1) / 4};
      ClientToScreen(hwnd, &point);
      COLORREF actual = GetPixel(dc, point.x, point.y);
      COLORREF expected = colors[phase][quadrant] & 0xFFFFFF;
      bool owned = WindowFromPoint(point) == hwnd;
      printf("Blit phase=%u image=%ux%u stride=%zu quadrant=%u owned=%d "
             "actual=%06lx expected=%06lx\n", phase, w, h, stride,
             quadrant, owned, actual, expected);
      passed = passed && owned && actual == expected;
    }
    ReleaseDC(nullptr, dc);
    if (!passed) return false;
  }
  return true;
}

static bool runAspectBlitTest(dante::NativeDevice& device, HWND hwnd) {
  const uint32_t aspects[][2] = {{16, 9}, {21, 9}, {4, 3}, {16, 10}};
  const uint32_t windows[][2] = {{960, 540}, {1260, 540}, {720, 540}, {864, 540}};
  const uint32_t colors[] = {0xFF808080, 0xFFC08040, 0xFF204060, 0xFFB0B0B0};
  uint32_t case_index = 0;
  for (uint32_t scale : {1u, 3u}) {
    const uint32_t src_w = 1280 * scale;
    const uint32_t src_h = 720 * scale;
    const size_t stride = src_w * 4 + 28;
    std::vector<uint8_t> source(stride * src_h, 0x7F);
    for (uint32_t y = 0; y < src_h; ++y) {
      for (uint32_t x = 0; x < src_w; ++x) {
        const uint32_t color = colors[(y >= src_h / 2) * 2 + (x >= src_w / 2)];
        std::memcpy(source.data() + y * stride + x * 4, &color, 4);
      }
    }
    for (const auto& aspect : aspects) {
      for (const auto& window : windows) {
        for (bool letterbox : {true, false}) {
          RECT outer{0, 0, LONG(window[0]), LONG(window[1])};
          AdjustWindowRect(&outer, WS_OVERLAPPEDWINDOW, FALSE);
          SetWindowPos(hwnd, HWND_TOPMOST, 40, 40,
                       outer.right - outer.left, outer.bottom - outer.top,
                       SWP_SHOWWINDOW);
          device.setDisplayAspect(double(aspect[0]) / aspect[1], letterbox);
          for (uint32_t frame = 0; frame < 8; ++frame) {
            MSG msg;
            while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
              if (msg.message == WM_QUIT) return false;
              TranslateMessage(&msg);
              DispatchMessageA(&msg);
            }
            if (!device.updateWindowSize() ||
                !device.presentImage(src_w, src_h, source.data(), stride, 1)) return false;
          }
          Sleep(100);
          const auto desc = device.swapchainDesc();
          if (desc.Width != window[0] || desc.Height != window[1]) return false;
          uint32_t width = window[0], height = window[1];
          if (letterbox) {
            if (uint64_t(width) * aspect[1] > uint64_t(height) * aspect[0]) {
              width = (uint64_t(height) * aspect[0] + aspect[1] / 2) / aspect[1];
            } else {
              height = (uint64_t(width) * aspect[1] + aspect[0] / 2) / aspect[0];
            }
          }
          const uint32_t left = (window[0] - width) / 2;
          const uint32_t top = (window[1] - height) / 2;
          POINT origin{};
          ClientToScreen(hwnd, &origin);
          for (uint32_t y = 8; y < window[1] - 8; y += 16) {
            for (uint32_t x = 8; x < window[0] - 8; x += 16) {
              if (WindowFromPoint(POINT{origin.x + LONG(x), origin.y + LONG(y)}) != hwnd) {
                printf("Aspect test occluded at %u,%u\n", x, y);
                return false;
              }
            }
          }
          HDC screen = GetDC(nullptr);
          HDC memory = CreateCompatibleDC(screen);
          BITMAPINFO info{};
          info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
          info.bmiHeader.biWidth = LONG(window[0]);
          info.bmiHeader.biHeight = -LONG(window[1]);
          info.bmiHeader.biPlanes = 1;
          info.bmiHeader.biBitCount = 32;
          info.bmiHeader.biCompression = BI_RGB;
          void* pixels = nullptr;
          HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
          HGDIOBJ old = SelectObject(memory, bitmap);
          const bool captured = BitBlt(memory, 0, 0, window[0], window[1], screen,
                                      origin.x, origin.y, SRCCOPY) && GdiFlush();
          uint64_t bad_bars = 0, bad_interior = 0, bars = 0, interior = 0;
          if (captured) {
            for (uint32_t y = 8; y < window[1] - 8; ++y) {
              for (uint32_t x = 8; x < window[0] - 8; ++x) {
                const uint32_t bgra = static_cast<const uint32_t*>(pixels)[y * window[0] + x];
                const uint32_t actual = ((bgra & 0xFF) << 16) | (bgra & 0xFF00) |
                                        ((bgra >> 16) & 0xFF);
                if (x < left || x >= left + width || y < top || y >= top + height) {
                  ++bars;
                  bad_bars += actual != 0;
                } else {
                  const int32_t dx = int32_t((x - left) * 2) - int32_t(width);
                  const int32_t dy = int32_t((y - top) * 2) - int32_t(height);
                  if (std::abs(dx) < 8 || std::abs(dy) < 8) continue;
                  const uint32_t expected = colors[(dy >= 0) * 2 + (dx >= 0)] & 0xFFFFFF;
                  ++interior;
                  bad_interior += actual != expected;
                }
              }
            }
            if ((scale == 3 && letterbox && window[0] == 1260) ||
                (scale == 1 && window[0] == 720 && !letterbox) || bad_bars || bad_interior) {
              char temp[MAX_PATH];
              char path[MAX_PATH];
              GetTempPathA(MAX_PATH, temp);
              std::snprintf(path, sizeof(path), "%sdante_blit_%lu_case%u.bmp",
                            temp, GetCurrentProcessId(), case_index);
              FILE* file = std::fopen(path, "wb");
              if (file) {
                BITMAPFILEHEADER header{};
                header.bfType = 0x4D42;
                header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
                header.bfSize = header.bfOffBits + window[0] * window[1] * 4;
                std::fwrite(&header, sizeof(header), 1, file);
                std::fwrite(&info.bmiHeader, sizeof(BITMAPINFOHEADER), 1, file);
                std::fwrite(pixels, window[0] * window[1] * 4, 1, file);
                std::fclose(file);
                printf("Aspect screenshot: %s\n", path);
              }
            }
          }
          SelectObject(memory, old);
          DeleteObject(bitmap);
          DeleteDC(memory);
          ReleaseDC(nullptr, screen);
          printf("Aspect case=%u scale=%u source=%ux%u display=%u:%u window=%ux%u "
                 "letterbox=%d rect=%u,%u,%u,%u bars=%llu bad_bars=%llu "
                 "interior=%llu bad_interior=%llu captured=%d\n",
                 case_index++, scale, src_w, src_h, aspect[0], aspect[1], window[0], window[1],
                 letterbox, left, top, width, height, bars, bad_bars, interior, bad_interior, captured);
          if (!captured || bad_bars || bad_interior || !interior) return false;
        }
      }
    }
  }
  ShowWindow(hwnd, SW_MINIMIZE);
  Sleep(100);
  if (device.updateWindowSize()) return false;
  ShowWindow(hwnd, SW_RESTORE);
  Sleep(100);
  if (!device.updateWindowSize()) return false;
  const uint32_t pixel = 0xFF808080;
  device.setDisplayAspect(16.0 / 9.0, false);
  if (!device.presentImage(1, 1, &pixel, 4, 1)) return false;
  if (device.presentImageShared(reinterpret_cast<void*>(1), 1280, 720, 0)) return false;
  printf("Aspect blit cases=%u minimize/restore and interop invalid-handle rejection passed\n", case_index);
  return true;
}

int main(int argc, char** argv) {
  SetProcessDPIAware();
  const char* class_name = "DanteNativeRendererTest";
  const uint32_t width = 1280;
  const uint32_t height = 720;

  WNDCLASSEXA wc = {};
  wc.cbSize = sizeof(WNDCLASSEXA);
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = GetModuleHandleA(nullptr);
  wc.lpszClassName = class_name;
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

  if (!RegisterClassExA(&wc)) {
    fprintf(stderr, "Failed to register window class\n");
    return 1;
  }

  RECT rc = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

  HWND hwnd = CreateWindowExA(0, class_name,
                              "Dante Native Renderer Test - UBO + Texture",
                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top,
                              nullptr, nullptr, wc.hInstance, nullptr);
  if (!hwnd) {
    fprintf(stderr, "Failed to create window\n");
    return 1;
  }

  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);

  dante::NativeDevice device;
  if (!device.initialize(hwnd, width, height)) {
    fprintf(stderr, "Failed to initialize native device\n");
    DestroyWindow(hwnd);
    return 1;
  }

  if (argc > 1 && std::strcmp(argv[1], "--blit-test") == 0) {
    const bool passed = runBlitTest(device, hwnd) && runAspectBlitTest(device, hwnd);
    device.shutdown();
    DestroyWindow(hwnd);
    UnregisterClassA(class_name, wc.hInstance);
    printf("Blit regression test %s\n", passed ? "passed" : "failed");
    return passed ? 0 : 1;
  }

  VertexTri tri_vertices[] = {
    {{ 0.0f,  0.5f}, {1.0f, 0.0f, 0.0f}},
    {{-0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
  };

  dante::BufferDesc vb_tri_desc;
  vb_tri_desc.size_bytes = sizeof(tri_vertices);
  dante::NativeBuffer* vb_tri = device.createBuffer(vb_tri_desc, tri_vertices);

  VertexQuad quad_vertices[] = {
    {{-0.7f,  0.2f}, {0.0f, 0.0f}},
    {{-0.7f, -0.2f}, {0.0f, 1.0f}},
    {{-0.3f,  0.2f}, {1.0f, 0.0f}},
    {{-0.3f, -0.2f}, {1.0f, 1.0f}},
  };

  dante::BufferDesc vb_quad_desc;
  vb_quad_desc.size_bytes = sizeof(quad_vertices);
  dante::NativeBuffer* vb_quad = device.createBuffer(vb_quad_desc, quad_vertices);

  dante::NativeBuffer* ubo = device.createUniformBuffer(sizeof(CameraCB));

  const uint32_t tex_w = 64, tex_h = 64;
  uint32_t* tex_pixels = new uint32_t[tex_w * tex_h];
  genCheckerboard(tex_pixels, tex_w, tex_h);

  dante::TextureDesc tex_desc;
  tex_desc.width = tex_w;
  tex_desc.height = tex_h;
  tex_desc.format = dante::TextureFormat::RGBA8_UNorm;
  tex_desc.initial_data = tex_pixels;
  tex_desc.data_size = tex_w * tex_h * 4;
  dante::NativeTexture* texture = device.createTexture(tex_desc);
  delete[] tex_pixels;

  dante::VertexElement tri_elems[] = {
    {dante::VertexSemantic::Position, dante::VertexFormat::Float32x2, 0, 0},
    {dante::VertexSemantic::Color,    dante::VertexFormat::Float32x3, 8, 0},
  };

  dante::PipelineDesc tri_pipe_desc;
  tri_pipe_desc.vertex_elements = tri_elems;
  tri_pipe_desc.vertex_element_count = 2;
  tri_pipe_desc.topology = dante::PrimitiveTopology::TriangleList;
  tri_pipe_desc.vertex_shader_source = kVSTri;
  tri_pipe_desc.pixel_shader_source = kPSTri;
  tri_pipe_desc.uniform_bindings = nullptr;
  tri_pipe_desc.uniform_binding_count = 0;

  dante::NativePipeline* tri_pipeline = device.createPipeline(tri_pipe_desc);

  dante::VertexElement quad_elems[] = {
    {dante::VertexSemantic::Position, dante::VertexFormat::Float32x2, 0, 0},
    {dante::VertexSemantic::TexCoord0, dante::VertexFormat::Float32x2, 8, 0},
  };

  dante::PipelineDesc quad_pipe_desc;
  quad_pipe_desc.vertex_elements = quad_elems;
  quad_pipe_desc.vertex_element_count = 2;
  quad_pipe_desc.topology = dante::PrimitiveTopology::TriangleStrip;
  quad_pipe_desc.vertex_shader_source = kVSQuad;
  quad_pipe_desc.pixel_shader_source = kPSQuad;
  quad_pipe_desc.uniform_bindings = nullptr;
  quad_pipe_desc.uniform_binding_count = 0;

  dante::NativePipeline* quad_pipeline = device.createPipeline(quad_pipe_desc);

  if (!vb_tri || !vb_quad || !ubo || !texture || !tri_pipeline || !quad_pipeline) {
    fprintf(stderr, "Failed to create resources\n");
    device.shutdown();
    DestroyWindow(hwnd);
    return 1;
  }

  device.bindPipeline(tri_pipeline);
  device.bindUniformBuffer(0, 0, ubo);

  device.bindPipeline(quad_pipeline);
  device.bindUniformBuffer(0, 0, ubo);
  device.bindTexture(1, 0, texture);

  printf("Native renderer test running.\n");
  printf("  Left: textured quad (checkerboard)\n");
  printf("  Right: rotating colored triangle (UBO transform)\n");
  printf("Press ESC to exit.\n");

  float t = 0.0f;
  bool running = true;
  while (running) {
    MSG msg;
    while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        running = false;
      }
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
    if (!running) break;

    t += 0.02f;

    CameraCB cb;
    buildTransformMatrix(t, 0.8f, 0.5f, 0.0f, cb.transform);
    device.updateUniformBuffer(ubo, &cb, sizeof(cb));

    device.beginFrame();
    device.clear(0.05f, 0.05f, 0.08f, 1.0f);

    device.bindPipeline(quad_pipeline);
    CameraCB quad_cb;
    buildTransformMatrix(0.0f, 1.0f, 0.0f, 0.0f, quad_cb.transform);
    device.updateUniformBuffer(ubo, &quad_cb, sizeof(quad_cb));
    device.bindVertexBuffer(0, vb_quad, sizeof(VertexQuad));
    device.draw(4);

    device.bindPipeline(tri_pipeline);
    buildTransformMatrix(t, 0.8f, 0.5f, 0.0f, cb.transform);
    device.updateUniformBuffer(ubo, &cb, sizeof(cb));
    device.bindVertexBuffer(0, vb_tri, sizeof(VertexTri));
    device.draw(3);

    device.present(1);
  }

  device.destroyPipeline(tri_pipeline);
  device.destroyPipeline(quad_pipeline);
  device.destroyTexture(texture);
  device.destroyBuffer(ubo);
  device.destroyBuffer(vb_quad);
  device.destroyBuffer(vb_tri);
  device.shutdown();
  DestroyWindow(hwnd);
  UnregisterClassA(class_name, wc.hInstance);

  printf("Native renderer test complete.\n");
  return 0;
}
