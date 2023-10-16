#ifndef includes
#define includes // syntax hightlighting
#include "../../../foray/src/shaders/common/lcrng.glsl"
#endif

// adapted from https://github.com/lukedan/ReSTIR-Vulkan/blob/master/src/shaders/

#define RESERVOIR_SIZE 4

struct LightSample {
	vec4 position_emissionLum;
	vec4 normal;
	uint lightIndex;
	float pHat;
	float sumWeights;
	float w;
};

struct Reservoir {
	LightSample samples[RESERVOIR_SIZE];
	uint numStreamSamples;
};

void updateReservoirAt(
	inout Reservoir res,
	int i,
	float weight,
	vec3 position,
	vec4 normal,
	float emissionLum,
	uint lightIdx,
	float pHat,
	float w,
	uint randomSeed
)
{
	res.samples[i].sumWeights += weight;
	float replacePossibility = weight / res.samples[i].sumWeights;
	float rand = lcgFloat(randomSeed);
	bool doReplace = rand < replacePossibility;
	
	if (doReplace)
	{
		res.samples[i].position_emissionLum = vec4(position, emissionLum);
		res.samples[i].normal = normal;
		res.samples[i].lightIndex = lightIdx;
		res.samples[i].pHat = pHat;
		res.samples[i].w = w;
	}
}

void addSampleToReservoir(inout Reservoir res, vec3 position, vec4 normal, float emissionLum,
						uint lightIdx, float pHat, float sampleP, uint randomSeed)
{

	float weight = pHat * sampleP;
	res.numStreamSamples += 1;
	for (int i = 0; i < RESERVOIR_SIZE; ++i)
	{
		float w = (res.samples[i].sumWeights + weight) / (res.numStreamSamples * pHat);
		randomSeed++;
		updateReservoirAt(res, i, weight, position, normal, emissionLum, lightIdx, pHat, w, randomSeed);
	}
}

void combineReservoirs(inout Reservoir self, Reservoir other, float pHat[RESERVOIR_SIZE], uint randomSeed) {
	self.numStreamSamples += other.numStreamSamples;
	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		randomSeed++;
		float weight = pHat[i] * other.samples[i].w * other.numStreamSamples;
		weight = clamp(weight, 1e-3, min(weight, 1.0f));
		if (weight > 0.0f) 
		{
			updateReservoirAt(
				self, i, weight,
				other.samples[i].position_emissionLum.xyz, other.samples[i].normal, other.samples[i].position_emissionLum.w,
				other.samples[i].lightIndex, pHat[i],
				other.samples[i].w, randomSeed
			);
		}
		if (self.samples[i].w > 0.0f) {
			self.samples[i].w = self.samples[i].sumWeights / (self.numStreamSamples * self.samples[i].pHat);
		}
	}
}
#define RESTIR_LIGHT_INDEX_INVALID 9999999

Reservoir newReservoir() {
	Reservoir result;
	for (int i = 0; i < RESERVOIR_SIZE; ++i)
	{
		result.samples[i].sumWeights = 0.0f;
		result.samples[i].pHat = 0.0f;
		result.samples[i].lightIndex = RESTIR_LIGHT_INDEX_INVALID;
	}
	result.numStreamSamples = 0u;
	return result;
}