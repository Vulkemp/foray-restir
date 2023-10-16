#version 450
#extension GL_KHR_vulkan_glsl : enable // Vulkan-specific syntax
#extension GL_GOOGLE_include_directive : enable // Include files
#extension GL_EXT_debug_printf : enable

// Camera Ubo
#define SET_CAMERA_UBO 0
#define BIND_CAMERA_UBO 2
#include "../../../foray/src/shaders/common/camera.glsl"

layout(location = 0) in vec3 inPosition;

vec2 positions[3] = vec2[](
    vec2(0.0, -0.5),
    vec2(0.5, 0.5),
    vec2(-0.5, 0.5)
);

vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 fragColor;

void main() {
	//debugPrintfEXT(" inPos %f %f %f \n", inPosition.x, inPosition.y, inPosition.z);
	//debugPrintfEXT("normalToLightFactor");
    gl_Position = Camera.ProjectionViewMatrix * vec4(inPosition, 1.f);
   // gl_Position = vec4(inPosition, 1.f);
	//gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
