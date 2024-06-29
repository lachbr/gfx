#include "material.hxx"

MaterialEnums::VertexColumnInfo MaterialEnums::vertex_column_info[] = {
    // VC_position
    {MaterialEnums::CT_float32, 3, false},
    // VC_texcoord
    {MaterialEnums::CT_float32, 2, false},
    // VC_normal
    {MaterialEnums::CT_float32, 3, false},
    // VC_tangent
    {MaterialEnums::CT_float32, 3, false},
    // VC_binormal
    {MaterialEnums::CT_float32, 3, false},
    // VC_color
    {MaterialEnums::CT_uint8, 4, true},
    // VC_joint_indices
    {MaterialEnums::CT_uint8, 4, false},
    // VCF_joint_weights
    {MaterialEnums::CT_float32, 4, false},
    // VCF_joint_indices2
    {MaterialEnums::CT_uint8, 4, false},
    // VCF_joint_weights2
    {MaterialEnums::CT_float32, 4, false},
    // VCF_user1
    {MaterialEnums::CT_float32, 4, false},
    // VCF_user2
    {MaterialEnums::CT_float32, 4, false},
};
