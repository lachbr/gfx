#version 430

layout(location = 0) in vec4 l_vtxColor;

layout(location = 0) out vec4 o_color;

void
main() {
  o_color = l_vtxColor;
}
