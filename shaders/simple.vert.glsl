#version 430

layout(location = 0) in vec3 vtx_position;
layout(location = 1) in vec2 vtx_texcoord;
layout(location = 2) in vec3 vtx_normal;

layout(std140, binding = 0) uniform CamParamsStruct {
  mat4 viewMatrix;
  mat4 projMatrix;
} camParams;

layout(location = 0) out vec4 l_vtxColor;

layout(push_constant) uniform Constants {
  mat4 modelMatrix;
} PushConstants;

void
main() {
  gl_Position = camParams.projMatrix * camParams.viewMatrix * PushConstants.modelMatrix * vec4(vtx_position, 1.0);
  l_vtxColor = vec4(vtx_normal * 0.5 + 0.5, 1.0);
}
