#version 460
#extension GL_KHR_vulkan_glsl : enable // Vulkan-specific syntax
#extension GL_GOOGLE_include_directive : enable // Include files
#extension GL_EXT_ray_tracing : enable // Raytracing
#extension GL_EXT_nonuniform_qualifier : enable // Required for asserting that some array indexing is done with non-uniform indices
#extension GL_EXT_debug_printf : enable

// Include structs and bindings
#include "rt_common/bindpoints.glsl"
#include "common/camera.glsl" // Binds camera matrices UBO
#include "rt_common/bindpoints.glsl" // Bindpoints (= descriptor set layout)
#include "common/materialbuffer.glsl" // Material buffer for material information and texture array
#include "rt_common/geometrymetabuffer.glsl" // GeometryMeta information
#include "rt_common/geobuffers.glsl" // Vertex and index buffer aswell as accessor methods
#include "common/normaltbn.glsl" // Normal calculation in tangent space
#include "common/lcrng.glsl"
#include "rt_common/tlas.glsl" // Binds Top Level Acceleration Structure


/// @brief Describes a simplified light source
struct Light  // std430
{
    /// @brief LightBall
    vec4 PositionAndRadius;
};

/// @brief Buffer containing array of simplified lights
layout(set = 0, binding = 11, std430) buffer readonly LightsBuffer
{
    /// @brief Array of simplifiedlight structures (guaranteed at minimum Count)
    Light Array[5];
}
Lights;


#include "shading/constants.glsl"
#include "shading/sampling.glsl"
#include "shading/material.glsl"

// Declare hitpayloads

#define HITPAYLOAD_IN
#define HITPAYLOAD_OUT
#include "rt_common/payload.glsl"
#define VISIPAYLOAD_OUT
#include "../visibilitytest/payload.glsl"

hitAttributeEXT vec2 attribs; // Barycentric coordinates

// Offsets a ray origin slightly away from the surface to prevent self shadowing
void CorrectOrigin(inout vec3 origin, vec3 normal, float nDotL)
{
    float correctorLength = clamp((1.0 - nDotL) * 0.005, 0, 1);
    origin += normal * correctorLength;
} 

vec2 hammersley2d(uint i, uint N) 
{
	// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
	uint bits = (i << 16u) | (i >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	float rdi = float(bits) * 2.3283064365386963e-10;
	return vec2(float(i) /float(N), rdi);
}

// sources for hemisphere sampling
// https://www.shadertoy.com/view/tltfWf Pick Points On Hemisphere
// https://github.com/SaschaWillems/Vulkan-glTF-PBR/blob/master/data/shaders/genbrdflut.frag
// https://www.shadertoy.com/view/4lscWj Hammersly Point Set
// unrelated
// https://alexanderameye.github.io/notes/sampling-the-hemisphere/
// https://schuttejoe.github.io/post/ggximportancesamplingpart1/


// uniform picking
vec3 hemiSpherePoint(vec3 normal, uint seed)
{
    float theta = 2.0 * PI * lcgFloat(seed);
    float cosPhi = lcgFloat(seed);
    float phi = acos(cosPhi);
    
    vec3 zAxis = normal;
	
    vec3 xAxis = normalize(cross(normal, abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0)));
    vec3 yAxis = normalize(cross(normal, xAxis));
    
    vec3 x = cos(theta) * xAxis;
    vec3 y = sin(theta) * yAxis;
    vec3 horizontal = normalize(x + y);
    vec3 z = cosPhi * zAxis;
    vec3 p = horizontal * sin(phi) + z;
    
    return normalize(p);
}

// cosine importance sampling
vec3 hemiSpherePointCos2(vec3 normal, uint seed)
{
    float theta = 2.0 * PI * lcgFloat(seed);
    float cosPhi = sqrt(sqrt(lcgFloat(seed)));
    float phi = acos(cosPhi);
    
    vec3 zAxis = normal;
	
    vec3 xAxis = normalize(cross(normal, abs(normal.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0)));
    vec3 yAxis = normalize(cross(normal, xAxis));
    
    vec3 x = cos(theta) * xAxis;
    vec3 y = sin(theta) * yAxis;
    vec3 horizontal = normalize(x + y);
    vec3 z = cosPhi * zAxis;
    vec3 p = horizontal * sin(phi) + z;
    
    return normalize(p);
}

vec4 CollectIncomingLightRandomHemiSphere(in vec3 pos, in vec3 normal, in vec3 outgoingDirection)
{
	float ndotl = dot(outgoingDirection, normal);
	vec3 origin = pos;
    CorrectOrigin(origin, normal, ndotl);

	ConstructHitPayload();
    ChildPayload.Seed = ReturnPayload.Seed + 1;
    ChildPayload.Depth = ReturnPayload.Depth + 1;

	traceRayEXT(MainTlas, // Top Level Acceleration Structure
                0, // RayFlags (Possible use: skip AnyHit, ClosestHit shaders etc.)
                0xff, // Culling Mask (Possible use: Skip intersection which don't have a specific bit set)
                0, // SBT record offset
                0, // SBT record stride
                0, // Miss Index
                origin, // Ray origin in world space
                0.001, // Minimum ray travel distance
                outgoingDirection, // Ray direction in world space
                INFINITY, // Maximum ray travel distance
                0 // Payload index (outgoing payload bound to location 0 in payload.glsl)
            );
			 
	return vec4(ChildPayload.Radiance, ChildPayload.Distance);

}

// Cosine distribution picking by iq
vec3 hemiSpherePointCos(vec3 normal, uint seed)
{
    float u = lcgFloat(seed);
    float v = lcgFloat(seed);
    float a = 6.2831853 * v;
    u = 2.0*u - 1.0;
    return normalize( normal + vec3(sqrt(1.0-u*u) * vec2(cos(a), sin(a)), u) );
}

// https://karthikkaranth.me/blog/generating-random-points-in-a-sphere/
vec3 getPointInSphere(float r, uint seed) 
{
    float u = lcgFloat(seed);
    float v = lcgFloat(seed);
    float theta = u * 2.0 * PI;
    float phi = acos(2.0 * v - 1.0);
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);
    float sinPhi = sin(phi);
    float cosPhi = cos(phi);
    float x = r * sinPhi * cosTheta;
    float y = r * sinPhi * sinTheta;
    float z = r * cosPhi;
    return vec3(x,y,z);
}

vec3 sampleLight(vec3 origin, uint seed)
{
	// our example scene has 5 static lights
	const uint numLights = 5;
	
	uint randomIndex = lcgUint(seed) % numLights;
	Light selectedLight = Lights.Array[randomIndex];

	vec3 pos = selectedLight.PositionAndRadius.yxz;
	// adapt to world
	vec3 realPos; 
	realPos.x = pos.y;
	realPos.y = -pos.x;
	realPos.z = -pos.z;  
	float radius = selectedLight.PositionAndRadius.w;

	// pick random point around center
	vec3 p = getPointInSphere(radius, seed);
	realPos += p;

	vec3 dir = realPos - origin;
	return normalize(dir);
}

void main()
{
	
	// this serves as a base to implement
	// - random monte carlo sampling
	// - importance sampling after the hemisphere
	// - importance sampling after the brdf
	// - importance sampling the lights

	// we need to
	// - generate a random variable
	
    // The closesthit shader is invoked with hit information on the geometry intersect closest to the ray origin
    
    // STEP #1 Get meta information on the intersected geometry (material) and the vertex information

    // Get geometry meta info
    GeometryMeta geometa = GetGeometryMeta(uint(gl_InstanceCustomIndexEXT), uint(gl_GeometryIndexEXT));
    MaterialBufferObject material = GetMaterialOrFallback(geometa.MaterialIndex);

    // get primitive indices
    const uvec3 indices = GetIndices(geometa, uint(gl_PrimitiveID));

    // get primitive vertices
    Vertex v0, v1, v2;
    GetVertices(indices, v0, v1, v2);

    // STEP #2 Calculate UV coordinates and probe the material

    // Calculate barycentric coords from hitAttribute values
    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

    // calculate uv
    const vec2 uv = v0.Uv * barycentricCoords.x + v1.Uv * barycentricCoords.y + v2.Uv * barycentricCoords.z;

    // Get material information at the current hitpoint
    MaterialProbe probe = ProbeMaterial(material, uv);

    // Calculate model and worldspace positions
    const vec3 posModelSpace = v0.Pos * barycentricCoords.x + v1.Pos * barycentricCoords.y + v2.Pos * barycentricCoords.z;
    const vec3 posWorldSpace = vec3(gl_ObjectToWorldEXT * vec4(posModelSpace, 1.f));

    // Interpolate normal of hitpoint
    const vec3 normalModelSpace = v0.Normal * barycentricCoords.x + v1.Normal * barycentricCoords.y + v2.Normal * barycentricCoords.z;
    const vec3 tangentModelSpace = v0.Tangent * barycentricCoords.x + v1.Tangent * barycentricCoords.y + v2.Tangent * barycentricCoords.z;
    const mat3 modelMatTransposedInverse = transpose(mat3(mat4x3(gl_WorldToObjectEXT)));
    vec3 normalWorldSpace = normalize(modelMatTransposedInverse * normalModelSpace);
    const vec3 tangentWorldSpace = normalize(tangentModelSpace);
    
    mat3 TBN = CalculateTBN(normalWorldSpace, tangentWorldSpace);

    normalWorldSpace = ApplyNormalMap(TBN, probe);

	 
	// evaluate material at hit point
	// sample_material(hitpoint, inDir, outDir, material, probe);
	vec3 brdf = vec3(1);

	// incoming light to current point
	vec3 Li = vec3(0);

    if (ReturnPayload.Depth < 1)
    {
		uint seed = ReturnPayload.Seed;
		// incoming light direction Wi w
		//vec3 Wi2 = hemiSpherePoint(normalWorldSpace, seed);
		//vec3 Wi2 = hemiSpherePointCos2(normalWorldSpace, seed);
        //vec3 Wi = hemiSpherePointCos(normalWorldSpace, seed); 

//		for(int i = 0; i < 3; i++)
//		{
//			vec3 ray = posWorldSpace - gl_WorldRayOriginEXT;
//			vec3 reflectedDir = normalize(reflect(ray, normalWorldSpace));
//
//			// sample after brdf
//			vec3 Wi2 = importanceSample_GGX(seed, probe.MetallicRoughness.y, reflectedDir);
//
//			// sample after Light
//			//vec3 Wi2 = sampleLight(posWorldSpace, seed);
//
//			vec3 Wi = vec3(-lcgFloat(seed), lcgFloat(seed), -lcgFloat(seed));
//
//			vec4 o = CollectIncomingLightRandomHemiSphere(posWorldSpace, normalWorldSpace, Wi);
//			vec4 o2 = CollectIncomingLightRandomHemiSphere(posWorldSpace, normalWorldSpace, Wi2);
//			//Li = o2.xyz / (o2.w*o2.w);
//			Li += o2.xyz;
//			//Li = (o.xyz / (o.w*o.w)+o2.xyz)/2;
//		}
//		Li /= 3.0f;

		vec3 ray = posWorldSpace - gl_WorldRayOriginEXT;
		vec3 reflectedDir = normalize(reflect(ray, normalWorldSpace));

		// sample after brdf
		//vec3 Wi2 = importanceSample_GGX(seed, probe.MetallicRoughness.y, reflectedDir);

		// sample after Light
		vec3 Wi2 = sampleLight(posWorldSpace, seed);

		vec3 Wi = vec3(-lcgFloat(seed), lcgFloat(seed), -lcgFloat(seed));

		vec4 o = CollectIncomingLightRandomHemiSphere(posWorldSpace, normalWorldSpace, Wi);
		vec4 o2 = CollectIncomingLightRandomHemiSphere(posWorldSpace, normalWorldSpace, Wi2);
		//Li = o2.xyz / (o2.w*o2.w);
		Li = o2.xyz / (o2.w*o2.w);
		//Li = (o.xyz / (o.w*o.w)+o2.xyz)/2;


		// evaluate brdf based in sample direction wo
		brdf = vec3(1); // TODO: sample_material(...)
    }

	// emissive light 
	vec3 Le = probe.EmissiveColor;

	vec3 baseColor = probe.BaseColor.xyz;
	

	// emitted outgoing radiance 
    float rayDist = length(posWorldSpace - gl_WorldRayOriginEXT);
	vec3 Lo = Li * brdf + Le;

	if(Le.x > 0 || Le.y > 0)
	{
		//debugPrintfEXT("Lo = %f, %f, %f \n", Lo.x, Lo.y, Lo.z);
		//float f = lcgFloat(ReturnPayload.Seed);
		//vec3 p = getPointInSphere(10.0, ReturnPayload.Seed);
		
		//debugPrintfEXT("point = %f, %f, %f \n", baseColor.x, baseColor.y, baseColor.z);
	}

    ReturnPayload.Radiance = Lo;
    ReturnPayload.Distance = length(posWorldSpace - gl_WorldRayOriginEXT);
}
