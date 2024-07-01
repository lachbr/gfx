#ifndef MATERIAL_HXX
#define MATERIAL_HXX

#include <vector>
#include <assert.h>
#include <algorithm>

#include "numeric_types.hxx"

using std::vector;

class MaterialEnums {
public:
  //
  // Vertex format stuff.
  //

  enum IndexType : uint8_t {
    IT_uint8,
    IT_uint16,
    IT_uint32,
  };

  enum PrimitiveTopology : uint8_t {
    PT_triangle_list,
    PT_triangle_strip,
    PT_line_list,
    PT_line_strip,
    PT_points,
  };

  enum ComponentType : uint8_t {
    CT_float32,
    CT_float16,
    CT_uint8,
  };

  enum VertexColumn : uint8_t {
    // X Y Z float
    VC_position,
    // X Y float
    VC_texcoord,
    // X Y Z float
    VC_normal,
    // X Y Z float
    VC_tangent,
    // X Y Z float
    VC_binormal,
    // RGBA8
    VC_color,

    //
    // GPU skinning vertex information.
    //
    // XYZW uint8
    VC_joint_indices,
    // XYZW float
    VC_joint_weights,
    // XYZW uint8
    VC_joint_indices2,
    // XYZW float
    VC_joint_weights2,
    // User-defined vertex columns.
    VC_user1,
    VC_user2,

    VC_COUNT,
  };

  // These are simply VertexColumn flags.
  typedef uint32_t VertexArrayFormat;

  //
  // Render state stuff.
  //

  enum StateFlags : uint32_t {
    SF_depth_write,
    SF_depth_test,
    SF_depth_offset,
    SF_cull_mode,
  };

  enum CompareOp : uint8_t {
    CO_none,
    CO_less,
    CO_equal,
    CO_less_equal,
    CO_greater,
    CO_greater_equal,
    CO_always,
  };

  enum BlendFactor : uint8_t {
    BF_zero,
    BF_one,
    BF_src_color,
    BF_one_minus_src_color,
    BF_dst_color,
    BF_one_minus_dst_color,
    BF_src_alpha,
    BF_one_minus_src_alpha,
    BF_dst_alpha,
    BF_one_minus_dst_alpha,
    BF_constant_color,
    BF_one_minus_constant_color,
    BF_constant_alpha,
    BF_one_minus_constant_alpha,
    BF_src_alpha_saturate,
    BF_src1_color,
    BF_one_minus_src1_color,
    BF_src1_alpha,
    BF_one_minus_src1_alpha,
  };

  enum BlendOp : uint8_t {
    BO_add,
    BO_sub,
    BO_reverse_sub,
    BO_min,
    BO_max,
  };

  enum CullMode : uint8_t {
    CM_none,
    CM_front,
    CM_back,
    CM_both,
  };

  enum RenderMode : uint8_t {
    RM_filled,
    RM_line,
    RM_point,
  };

  enum TransparencyMode : uint8_t {
    TM_none,
    TM_alpha_blend,
    TM_alpha_test,
  };

public:
  struct VertexColumnInfo {
    ComponentType component_type;
    int num_components;
    bool normalized;
  };

  static VertexColumnInfo vertex_column_info[VC_COUNT];

public:
  static ubyte index_type_size(IndexType type) {
    switch (type) {
    case IT_uint8:
      return 1u;
    case IT_uint16:
      return 2u;
    case IT_uint32:
      return 4u;
    }
  }

  static ubyte component_type_size(ComponentType type) {
    switch (type) {
    case CT_float32:
      return 4u;
    case CT_float16:
      return 2u;
    case CT_uint8:
      return 1u;
    }
  }

  static uint32_t vertex_column_flag(VertexColumn column) {
    return 1u << (uint32_t)column;
  }

  inline static uint8_t vertex_column_component_size(VertexColumn c) {
    return component_type_size(vertex_column_info[c].component_type);
  }

  inline static uint8_t vertex_column_stride(VertexColumn c) {
    const VertexColumnInfo &cinfo = vertex_column_info[c];
    return cinfo.num_components * component_type_size(cinfo.component_type);
  }

  inline static size_t vertex_row_stride(VertexArrayFormat format) {
    size_t stride = 0u;
    for (int i = 0; i < (int)VC_COUNT; ++i) {
      if (format & (1 << i)) {
        stride += vertex_column_stride((VertexColumn)i);
      }
    }
    return stride;
  }

  inline static size_t vertex_column_offset(VertexArrayFormat format, VertexColumn c) {
    size_t offset = 0u;
    for (int i = 0; i < (int)VC_COUNT && i < (int)c; ++i) {
      if (format & (1 << i)) {
        offset += vertex_column_stride((VertexColumn)i);
      }
    }
    return offset;
  }

};

struct VertexFormat {
  vector<MaterialEnums::VertexArrayFormat> arrays;
};

struct VertexData {
  VertexFormat format;
  vector<vector<ubyte>> array_buffers;

  inline int get_num_vertices() const {
    return array_buffers[0].size() / MaterialEnums::vertex_row_stride(format.arrays[0]);
  }
};

struct IndexData {
  MaterialEnums::IndexType type;
  vector<ubyte> buffer;

  inline int get_num_indices() const {
    return buffer.size() / MaterialEnums::index_type_size(type);
  }
};

class IndexWriter {
public:
  IndexWriter(IndexData *idata) {
    _buf = &idata->buffer;
    _data = idata;
  }

  inline void set_row(int row) {
    _position = row * MaterialEnums::index_type_size(_data->type);
  }

  inline void reserve_num_rows(int count) {
    _buf->reserve(count * MaterialEnums::index_type_size(_data->type));
  }

  inline void set_num_rows(int count) {
    _buf->resize(count * MaterialEnums::index_type_size(_data->type));
  }

  inline void inc_ptr() {
    _position += MaterialEnums::index_type_size(_data->type);
  }

  inline void ensure_buf_size() {
    if (_position >= _buf->size()) {
      _buf->resize(_position + MaterialEnums::index_type_size(_data->type));
    }
  }

  inline void write(u32 val) {
    switch (_data->type) {
    case MaterialEnums::IT_uint8:
      *(u8 *)&_buf->at(_position) = val;
      break;
    case MaterialEnums::IT_uint16:
      *(u16 *)&_buf->at(_position) = val;
      break;
    case MaterialEnums::IT_uint32:
      *(u32 *)&_buf->at(_position) = val;
      break;
    }
    inc_ptr();
  }

  inline void write(u32 v1, u32 v2, u32 v3) {
    write(v1);
    write(v2);
    write(v3);
  }

  inline void write_v(u32 *vals, int count) {
    for (int i = 0; i < count; ++i) {
      write(vals[i]);
    }
  }

  inline void add(u32 val) {
    ensure_buf_size();
    write(val);
  }

  inline void add(u32 v1, u32 v2, u32 v3) {
    ensure_buf_size();
    write(v1);
    ensure_buf_size();
    write(v2);
    ensure_buf_size();
    write(v3);
  }

  inline void add_v(u32 *vals, int count) {
    for (int i = 0; i < count; ++i) {
      ensure_buf_size();
      write(vals[i]);
    }
  }

private:
  const IndexData *_data;
  vector<ubyte> *_buf;
  size_t _position;
};

// Helper class to write vertex data.
// The set_* methods do not resize the buffer, so use if you know the size
// upfront or know that you're not going past the end of the buffer.
// The add_* methods resize the buffer if you try to write past the end.
class VertexWriter {
public:
  VertexWriter(VertexData *vdata, MaterialEnums::VertexColumn c) {
    int array = -1;
    // Find the array with the column.
    for (int i = 0; i < vdata->format.arrays.size(); ++i) {
      if (vdata->format.arrays[i] & (1 << c)) {
        array = i;
        break;
      }
    }

    assert(array != -1);

    // Note location in buffer/sizing.
    _row_stride = MaterialEnums::vertex_row_stride(vdata->format.arrays[array]);
    _position =
        MaterialEnums::vertex_column_offset(vdata->format.arrays[array], c);
    _offset = _position;
    _buf = &vdata->array_buffers[array];
    _column_info = &MaterialEnums::vertex_column_info[c];
  }

  inline ubyte *data(ubyte ofs = 0u) { return &_buf->at(_position + ofs); }

  inline void inc_ptr() { _position += _row_stride; }

  inline void set_row(int row) {
    _position = row * _row_stride + _offset;
  }

  inline void write_data_iv(int *vals, int count) {
    switch (_column_info->component_type) {

    case MaterialEnums::CT_float32: {
      float *ptr = (float *)data();
      for (int i = 0; i < _column_info->num_components && i < count; ++i) {
        ptr[i] = (float)vals[i];
      }
      break;
    }

    case MaterialEnums::CT_float16:
      assert(false);
      break;

    case MaterialEnums::CT_uint8: {
      ubyte *ptr = data();
      for (int i = 0; i < _column_info->num_components && i < count; ++i) {
        ptr[i] = vals[i];
      }
      break;
    }
    }
  }

  inline void write_data_fv(float *vals, int count) {
    switch (_column_info->component_type) {

    case MaterialEnums::CT_float32: {
      float *ptr = (float *)data();
      for (int i = 0; i < _column_info->num_components && i < count; ++i) {
        ptr[i] = vals[i];
      }
      break;
    }

    case MaterialEnums::CT_float16:
      assert(false);
      break;

    case MaterialEnums::CT_uint8: {
      ubyte *ptr = data();
      for (int i = 0; i < _column_info->num_components && i < count; ++i) {
        if (_column_info->normalized) {
          ptr[i] = std::clamp((u8)(vals[i] * 255.0f), (u8)0u, (u8)255u);
        } else {
          ptr[i] = (u8)vals[i];
        }
      }
      break;
    }
    }
  }

  inline void set_data_1i(int val) {
    write_data_iv(&val, 1);
    inc_ptr();
  }

  inline void set_data_2i(int val1, int val2) {
    int vals[2] = { val1, val2 };
    write_data_iv(vals, 2);
    inc_ptr();
  }

  inline void set_data_3i(int val1, int val2, int val3) {
    int vals[3] = {val1, val2, val3};
    write_data_iv(vals, 3);
    inc_ptr();
  }

  inline void set_data_4i(int val1, int val2, int val3, int val4) {
    int vals[4] = {val1, val2, val3, val4};
    write_data_iv(vals, 4);
    inc_ptr();
  }

  inline void set_data_1f(float val) {
    write_data_fv(&val, 1);
    inc_ptr();
  }

  inline void set_data_2f(float val1, float val2) {
    float vals[2] = {val1, val2};
    write_data_fv(vals, 2);
    inc_ptr();
  }

  inline void set_data_3f(float val1, float val2, float val3) {
    float vals[3] = {val1, val2, val3};
    write_data_fv(vals, 3);
    inc_ptr();
  }

  inline void set_data_4f(float val1, float val2, float val3, float val4) {
    float vals[4] = {val1, val2, val3, val4};
    write_data_fv(vals, 4);
    inc_ptr();
  }

  inline bool is_at_end() const { return _position >= _buf->size(); }

  inline void set_num_rows(int count) {
    _buf->resize(count * _row_stride);
  }

  inline void ensure_buf_size() {
    if (is_at_end()) {
      _buf->resize(_position - _offset + _row_stride);
    }
  }

  inline void add_data_1i(int val) {
    ensure_buf_size();
    set_data_1i(val);
  }

  inline void add_data_2i(int val1, int val2) {
    ensure_buf_size();
    set_data_2i(val1, val2);
  }

  inline void add_data_3i(int val1, int val2, int val3) {
    ensure_buf_size();
    set_data_3i(val1, val2, val3);
  }

  inline void add_data_4i(int val1, int val2, int val3, int val4) {
    ensure_buf_size();
    set_data_4i(val1, val2, val3, val4);
  }

  inline void add_data_1f(float val) {
    ensure_buf_size();
    set_data_1f(val);
  }

  inline void add_data_2f(float val1, float val2) {
    ensure_buf_size();
    set_data_2f(val1, val2);
  }

  inline void add_data_3f(float val1, float val2, float val3) {
    ensure_buf_size();
    set_data_3f(val1, val2, val3);
  }

  inline void add_data_4f(float val1, float val2, float val3, float val4) {
    ensure_buf_size();
    set_data_4f(val1, val2, val3, val4);
  }

private:
  vector<ubyte> *_buf;
  const MaterialEnums::VertexColumnInfo *_column_info;
  int _offset;
  int _row_stride;
  // Byte position.
  size_t _position;
};

// A mesh is simply a reference to a vertex buffer and an optional index buffer,
// along with a primitive toplogy.
//
// The mesh can specify a subset of the vertex buffer (if not indexed) or the index
// buffer to render from.
struct Mesh {
  const VertexData *vertex_data;
  const IndexData *index_data;
  uint32_t first_vertex;
  uint32_t num_vertices;
  MaterialEnums::PrimitiveTopology topology;

  inline bool is_indexed() const {
    return index_data != nullptr;
  }
};

class Renderer {
};

// A shader implementation is responsible for supplying a graphics pipeline
// state from a material and mesh vertex format.
//
// It typically also corresponds to a particular set of shader modules.
class Shader {
public:
  //virtual
};

// Fixed material data.
struct StaticMaterialData : public MaterialEnums {
  Shader *_shader;
  unsigned int _state_flags;
  float _line_width;
  float _depth_bias;
  float _alpha_test_ref;
  CompareOp _depth_test_func;
  CullMode _cull_mode;
  RenderMode _render_mode;
  TransparencyMode _transparency;
  CompareOp _alpha_test_func;
  bool _depth_write;
};

// A material defines parameters to a particular Shader implementation.
// It may also define fixed-function render state params, though it's up
// to the Shader whether or not it is respected.
class Material : public MaterialEnums {
public:

public:
  inline Shader *get_shader() const { return _shader; }

  inline StaticMaterialData *get_static_data() const { return _static_data; }

private:
  const StaticMaterialData *_static_data;
};

#endif // MATERIAL_HXX
