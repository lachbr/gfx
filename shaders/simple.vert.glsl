#version 430

layout(location = 0) in vec4 vtx_position;
layout(location = 1) in vec4 vtx_color;

layout(std140, binding = 0) uniform CamParamsStruct {
  mat4 modelMatrix;
  mat4 viewMatrix;
  mat4 projMatrix;
} camParams;

layout(location = 0) out vec4 l_vtxColor;

void
main() {
  gl_Position = camParams.projMatrix * camParams.viewMatrix * camParams.modelMatrix * vtx_position;
  l_vtxColor = vtx_color;
}
