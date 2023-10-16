#version 450

layout(location = 0) out vec4 outColor;
#extension GL_EXT_debug_printf : enable
layout(location = 0) in vec3 fragColor;
void main() {
     outColor = vec4(1.0,0,0,0);
}