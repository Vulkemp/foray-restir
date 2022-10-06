#version 460
#extension GL_GOOGLE_include_directive : enable // Include files
#extension GL_EXT_ray_tracing : enable // Raytracing

// Declare hitpayloads

#define HITPAYLOAD_IN
#include "../../../hsk_rt_rpf/src/shaders/rt_common/bindpoints.glsl"
#include "../../../hsk_rt_rpf/src/shaders/rt_common/payload.glsl"
#include "../../../hsk_rt_rpf/src/shaders/common/environmentmap.glsl"

void main()
{
    ReturnPayload.Radiance = SampleEnvironmentMap(gl_WorldRayDirectionEXT).xyz;
}