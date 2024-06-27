#include "vulkan/vulkan_core.h"
#include <iostream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <unordered_set>

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef uint64_t u64;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;
typedef float f32;
typedef double f64;

using std::vector;

#undef UNICODE
#define NOMINMAX
#include <windows.h>

#include "renderer.hxx"

#include "linmath.hxx"

// Windows
WNDCLASS wc = { };
const char *wnd_class_name = "gfxwndclass";
HWND hwnd = nullptr;
bool window_closed = false;

LRESULT APIENTRY
window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  switch (uMsg) {
  case WM_CLOSE:
    DestroyWindow(hwnd);
    return 0;
  case WM_DESTROY:
    std::cerr << "Window destroyed\n";
    window_closed = true;
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void
make_window_class() {
  wc.lpfnWndProc = window_proc;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.lpszClassName = wnd_class_name;
  RegisterClass(&wc);
}

void
make_window() {
  hwnd = CreateWindowEx(0, wnd_class_name, "Window", WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                        nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  assert(hwnd != nullptr);
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);
}

void
update_window() {
  MSG msg;
  if (PeekMessage(&msg, hwnd, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

std::unordered_set<VertexData *> queued_vertex_data;
VkVertexBuffer *vk_buf;

void
render_frame(RendererVk *render) {
  render->begin_prepare();
  if (queued_vertex_data.size() > 0u) {
    for (VertexData *data : queued_vertex_data) {
      render->prepare_vertex_data(data, &vk_buf);
    }
    queued_vertex_data.clear();
  }
  render->end_prepare();

  render->begin_frame();

  render->begin_frame_surface();

  render->draw(&vk_buf, 1, 0, 3);

  render->end_frame_surface();

  render->end_frame();
}

int
main(int argc, char *argv[]) {
  make_window_class();
  make_window();

  RendererVk render;
  if (!render.initialize(hwnd)) {
    return 1;
  }

  VertexData vdata;
  vdata.format.num_arrays = 1;
  vdata.format.array_formats = { MaterialEnums::vertex_column_flag(MaterialEnums::VC_position) | MaterialEnums::vertex_column_flag(MaterialEnums::VC_color) };
  vdata.array_buffers.resize(1);
  vdata.array_buffers[0].resize(sizeof(float) * 8 * 3);
  float *fdata = (float *)vdata.array_buffers[0].data();
  fdata[0] = -1.0f;
  fdata[1] = 20.0f;
  fdata[2] = -1.0f;
  fdata[3] = 1.0f;
  fdata[4] = 1.0f;
  fdata[5] = 0.0f;
  fdata[6] = 0.0f;
  fdata[7] = 1.0f;
  fdata[8] = 1.0f;
  fdata[9] = 20.0f;
  fdata[10] = -1.0f;
  fdata[11] = 1.0f;
  fdata[12] = 0.0f;
  fdata[13] = 1.0f;
  fdata[14] = 0.0f;
  fdata[15] = 1.0f;
  fdata[16] = 1.0f;
  fdata[17] = 20.0f;
  fdata[18] = 1.0f;
  fdata[19] = 1.0f;
  fdata[20] = 0.0f;
  fdata[21] = 0.0f;
  fdata[22] = 1.0f;
  fdata[23] = 1.0f;

  queued_vertex_data.insert(&vdata);

  while (!window_closed) {
    update_window();
    render_frame(&render);
  }

  return 0;
}
