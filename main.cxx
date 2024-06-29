#include "vulkan/vulkan_core.h"
#include <iostream>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <unordered_set>
#include <map>
#include <sstream>

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
#include "obj_reader.hxx"

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
std::unordered_set<IndexData *> queued_index_data;

std::vector<Mesh> make_obj_meshes(const std::string &filename, RendererVk *render) {
  std::vector<Mesh> out;

  std::ifstream stream(filename);
  if (!stream.good()) {
    return out;
  }

  std::ostringstream ss;
  ss << stream.rdbuf();
  std::string data = ss.str();

  ObjReader reader(data);

  struct VertexKey {
    float vertex[4];
    float normal[3];
    float texcoord[2];

    bool operator<(const VertexKey &other) const {
      for (int i = 0; i < 4; ++i) {
        if (vertex[i] < other.vertex[i]) {
          return true;
        }
      }
      for (int i = 0; i < 3; ++i) {
        if (normal[i] < other.normal[i]) {
          return true;
        }
      }
      for (int i = 0; i < 2; ++i) {
        if (texcoord[i] < other.texcoord[i]) {
          return true;
        }
      }
      return false;
    }
  };
  std::map<VertexKey, int> vertex_map;
  std::map<const ObjFaceVert *, int> face_vert_map;

  int vtx_count = 0;
  int index_count = 0;
  for (const ObjObject &obj : reader.objects) {
    for (const ObjFace &face : obj.faces) {
      index_count += (face.verts.size() - 2) * 3;
      for (const ObjFaceVert &vert : face.verts) {
        VertexKey key;
        key.vertex[0] = reader.vertex[vert.vertex][0];
        key.vertex[1] = reader.vertex[vert.vertex][2];
        key.vertex[2] = reader.vertex[vert.vertex][1];
        //float *pos = reader.vertex[vert.vertex];
        key.normal[0] = 0.0f;
        key.normal[1] = 0.0f;
        key.normal[2] = 0.0f;
        if (vert.normal != -1) {
          key.normal[0] = reader.normal[vert.normal][0];
          key.normal[1] = reader.normal[vert.normal][2];
          key.normal[2] = reader.normal[vert.normal][1];
        }
        key.texcoord[0] = 0.0f;
        key.texcoord[1] = 0.0f;
        if (vert.texcoord != -1) {
          key.texcoord[0] = reader.texcoord[vert.texcoord][0];
          key.texcoord[1] = reader.texcoord[vert.texcoord][1];
        }
        auto it = vertex_map.find(key);
        if (it == vertex_map.end()) {
          face_vert_map.insert({ &vert, vtx_count });
          vertex_map.insert({ key, vtx_count++ });
        } else {
          face_vert_map.insert({ &vert, it->second });
        }
      }
    }
  }

  MaterialEnums::VertexArrayFormat format = 0u;
  if (reader.vertex.size() > 0u) {
    format |= MaterialEnums::vertex_column_flag(MaterialEnums::VC_position);
  }
  if (reader.normal.size() > 0u) {
    format |= MaterialEnums::vertex_column_flag(MaterialEnums::VC_normal);
  }
  if (reader.texcoord.size() > 0u) {
    format |= MaterialEnums::vertex_column_flag(MaterialEnums::VC_texcoord);
  }
  VertexData *vdata = render->make_vertex_data({{format}});

  VertexWriter vwriter(vdata, MaterialEnums::VC_position);
  vwriter.set_num_rows(vertex_map.size());

  for (auto it : vertex_map) {
    vwriter.set_row(it.second);
    vwriter.set_data_3f(it.first.vertex[0], it.first.vertex[1],
                        it.first.vertex[2]);
  }

  std::cout << "vertex map:\n";
  for (auto it : vertex_map) {
    std::cout << it.first.vertex[0] << ", " << it.first.vertex[1] << ", " << it.first.vertex[2] << "\n";
  }

  std::cout << "texcoord map:\n";
  for (auto it : vertex_map) {
    std::cout << it.first.texcoord[0] << ", " << it.first.texcoord[1] << "\n";
  }

  std::cout << "normal map:\n";
  for (auto it : vertex_map) {
    std::cout << it.first.normal[0] << ", " << it.first.normal[1] << ", " << it.first.normal[2] << "\n";
  }

  if (reader.normal.size() > 0u) {
    VertexWriter nwriter(vdata, MaterialEnums::VC_normal);
    for (auto it : vertex_map) {
      nwriter.set_row(it.second);
      nwriter.set_data_3f(it.first.normal[0], it.first.normal[1],
                          it.first.normal[2]);
    }
  }

  if (reader.texcoord.size() > 0u) {
    VertexWriter twriter(vdata, MaterialEnums::VC_texcoord);
    for (auto it : vertex_map) {
      twriter.set_row(it.second);
      twriter.set_data_2f(it.first.texcoord[0], it.first.texcoord[1]);
    }
  }

  // Now build indices
  IndexData *idata = render->make_index_data(MaterialEnums::IT_uint16);
  idata->buffer.resize(index_count * sizeof(u16));
  u16 *iptr = (u16 *)idata->buffer.data();
  int index_ptr = 0;
  for (ObjObject &obj : reader.objects) {
    Mesh m;
    m.vertex_data = vdata;
    m.index_data = idata;
    m.first_vertex = index_ptr;
    for (ObjFace &face : obj.faces) {
      for (int i = 0; i < face.verts.size() - 2; ++i) {
        *iptr++ = face_vert_map[&face.verts[0]];
        *iptr++ = face_vert_map[&face.verts[i + 1]];
        *iptr++ = face_vert_map[&face.verts[i + 2]];
        index_ptr += 3;
      }
    }
    m.num_vertices = index_ptr - m.first_vertex;
    m.topology = MaterialEnums::PT_triangle_list;
    out.push_back(std::move(m));
  }

  queued_vertex_data.insert(vdata);
  queued_index_data.insert(idata);

  return out;
}

std::vector<Mesh> meshes;

void
render_frame(RendererVk *render) {
  render->begin_prepare();
  if (queued_vertex_data.size() > 0u) {
    for (VertexData *data : queued_vertex_data) {
      render->prepare_vertex_data(data);
    }
    queued_vertex_data.clear();
  }
  if (queued_index_data.size() > 0u) {
    for (IndexData *data : queued_index_data) {
      render->prepare_index_data(data);
    }
    queued_index_data.clear();
  }
  render->end_prepare();

  render->begin_frame();

  render->begin_frame_surface();

  for (const Mesh &mesh : meshes) {
    render->draw_mesh(&mesh);
  }

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

  meshes = make_obj_meshes("models\\cottage_obj.obj", &render);

  while (!window_closed) {
    update_window();
    render_frame(&render);
  }

  return 0;
}
