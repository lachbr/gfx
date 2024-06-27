#include "material.hxx"

MaterialEnums::VertexColumnFormatInfo MaterialEnums::vertex_column_format_info[] = {
  // VCF_r32g32b32a32_float
  {
    4, sizeof(float),
  },
  // VCF_r32g32b32_float
  {
    3, sizeof(float),
  },
  // VCF_r32g32_float
  {
    2, sizeof(float),
  },
  // VCF_r32_float
  {
    1, sizeof(float),
  },
  // VCF_r16g16b16_float
  {
    3, sizeof(int16_t),
  },
  // VCF_r16_float
  {
    1, sizeof(int16_t),
  },
  // VCF_r8g8b8a8_unorm
  {
    4, sizeof(unsigned char),
  },
  // VCF_r8g8b8a8_uint
  {
    4, sizeof(unsigned char),
  },
  // VCF_r8_unorm
  {
    1, sizeof(unsigned char),
  },
};

MaterialEnums::VertexColumnInfo MaterialEnums::vertex_column_info[] = {
  // VC_position
  {
    VCF_r32g32b32_float,
  },
  // VC_color
  {
    VCF_r8g8b8a8_unorm,
  },
  // VC_texcoord
  {
    VCF_r32g32_float,
  },
  // VC_normal
  {
    VCF_r32g32b32_float,
  },
  // VC_tangent
  {
    VCF_r32g32b32_float,
  },
  // VC_binormal
  {
    VCF_r32g32b32_float,
  },
  // VC_joint_indices
  {
    VCF_r8g8b8a8_uint,
  },
  // VCF_joint_weights
  {
    VCF_r32g32b32a32_float,
  },
  // VCF_joint_indices2
  {
    VCF_r8g8b8a8_uint,
  },
  // VCF_joint_weights2
  {
    VCF_r8g8b8a8_uint,
  },
  // VCF_user1
  {
    VCF_r32g32b32a32_float,
  },
  // VCF_user2
  {
    VCF_r32g32b32a32_float,
  },
};

MaterialEnums::IndexFormatInfo MaterialEnums::index_format_info[] = {
  { 1 },
  { 2 },
  { 4 }
};
