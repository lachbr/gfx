#ifndef MATERIAL_HXX
#define MATERIAL_HXX

#include <vector>
#include <assert.h>

using std::vector;

class MaterialEnums {
public:
  //
  // Vertex format stuff.
  //

  enum IndexFormat : uint8_t {
    IF_uint8,
    IF_uint16,
    IF_uint32,

    IF_COUNT,
  };

  enum PrimitiveTopology : uint8_t {
    PT_triangle_list,
    PT_triangle_strip,
    PT_line_list,
    PT_line_strip,
    PT_points,
  };

  enum VertexColumnFormat : uint8_t {
    VCF_r32g32b32a32_float,
    VCF_r32g32b32_float,
    VCF_r32g32_float,
    VCF_r32_float,
    VCF_r16g16b16_float,
    VCF_r16_float,
    VCF_r8g8b8a8_unorm,
    VCF_r8g8b8a8_uint,
    VCF_r8_unorm,

    VCF_COUNT,
  };

  enum VertexColumn : uint8_t {
    // X Y Z float
    VC_position,
    // RGBA8
    VC_color,
    // X Y float
    VC_texcoord,
    // X Y Z float
    VC_normal,
    // X Y Z float
    VC_tangent,
    // X Y Z float
    VC_binormal,
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
  struct VertexColumnFormatInfo {
    int num_components;
    // Byte size per component.
    uint8_t component_size;
  };

  struct VertexColumnInfo {
    VertexColumnFormat format;
  };

  struct IndexFormatInfo {
    uint8_t stride;
  };

public:

  static VertexColumnFormatInfo vertex_column_format_info[VCF_COUNT];
  static VertexColumnInfo vertex_column_info[VC_COUNT];

  static IndexFormatInfo index_format_info[3];

  static uint32_t vertex_column_flag(VertexColumn column) {
    return 1u << (uint32_t)column;
  }

  inline static uint8_t index_format_stride(IndexFormat fmt) {
    return index_format_info[(int)fmt].stride;
  }

  inline static uint8_t vertex_column_component_size(VertexColumn c) {
    return vertex_column_format_info[vertex_column_info[(int)c].format].component_size;
  }

  inline static uint8_t vertex_column_stride(VertexColumn c) {
    const VertexColumnInfo &cinfo = vertex_column_info[(int)c];
    const VertexColumnFormatInfo &cfinfo = vertex_column_format_info[(int)cinfo.format];
    return cfinfo.num_components * cfinfo.component_size;
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
      offset += vertex_column_stride((VertexColumn)i);
    }
    return offset;
  }

};

struct VertexFormat {
  vector<MaterialEnums::VertexArrayFormat> arrays;
};

struct VertexData {
  VertexFormat format;
  vector<vector<unsigned char>> array_buffers;

  inline int get_num_vertices() const {
    return array_buffers[0].size() / MaterialEnums::vertex_row_stride(format.arrays[0]);
  }
};

struct IndexData {
  MaterialEnums::IndexFormat format;
  vector<unsigned char> buffer;

  inline int get_num_indices() const {
    return buffer.size() / MaterialEnums::index_format_stride(format);
  }
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

// A shader implementation is responsible for supplying a graphics pipeline
// state from a material and mesh vertex format.
//
// It typically also corresponds to a particular set of shader modules.
class Shader {

};

// A material defines parameters to a particular Shader implementation.
// It may also define fixed-function render state params, though it's up
// to the Shader whether or not it is respected.
class Material : public MaterialEnums {
public:



public:
  inline Shader *get_shader() const { return _shader; }

private:
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

#endif // MATERIAL_HXX
