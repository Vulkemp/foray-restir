#version 460
#extension GL_GOOGLE_include_directive : enable // Include files
#extension GL_EXT_ray_tracing : enable // Raytracing

// Declare hitpayloads

#define HITPAYLOAD_IN
#include "../../../foray/src/shaders/rt_common/bindpoints.glsl"
#include "../../../foray/src/shaders/rt_common/payload.glsl"
#include "../../../foray/src/shaders/common/environmentmap.glsl"

void main()
{
    ReturnPayload.Radiance = SampleEnvironmentMap(gl_WorldRayDirectionEXT).xyz;
}