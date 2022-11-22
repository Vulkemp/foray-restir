#ifndef includes
#define includes // syntax hightlighting
#include "../../../foray/src/shaders/common/lcrng.glsl"
#endif

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
	uint randomSeed,
	bool activatePrintf
)
{
	res.samples[i].sumWeights += weight;
	float replacePossibility = weight / res.samples[i].sumWeights;
	bool doReplace = lcgFloat(randomSeed) < replacePossibility;

	if(activatePrintf)
	{
		debugPrintfEXT("updateReservoirAt replacement: %d, rep prob %f, lightIndex %d => %d, pHat %f = %f, absolute weight %f, \n",
		doReplace,
		replacePossibility,
		res.samples[i].lightIndex,
		lightIdx,
		res.samples[i].pHat,
		pHat,
		weight);
	}

	if (doReplace) {
		res.samples[i].position_emissionLum = vec4(position, emissionLum);
		res.samples[i].normal = normal;
		res.samples[i].lightIndex = lightIdx;
		res.samples[i].pHat = pHat;
		res.samples[i].w = w;
	}
}
//addSampleToReservoir(res, lightSamplePos, lightNormal, lightSampleLum, selected_idx, pHat, lightSampleProb, randomSeed);
void addSampleToReservoir(inout Reservoir res, vec3 position, vec4 normal, float emissionLum, uint lightIdx, float pHat, float sampleP, uint randomSeed, bool activatePrintf) {
	
	if(activatePrintf)
	{
	debugPrintfEXT("add sample to reservoir with sampleP %f \n", sampleP);
	}
	// weight is defined by light Strength, divided by light probability.
	// (light prob. based on normal angle, triangle area and emissive factor)
	// weight = absolute weight of how good we consider a sample.
	float weight = pHat * sampleP;
	res.numStreamSamples += 1;
	//debugPrintfEXT(" absolute sample weight = %f \n", weight);
	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		// the probability to chose the sample as new sample is
		// w is average weight, bcs we add sum of all weights and divide by number of elements, multiplied by light power
		float w = (res.samples[i].sumWeights + weight) / (res.numStreamSamples * pHat);
		updateReservoirAt(res, i, weight, position, normal, emissionLum, lightIdx, pHat, w, randomSeed, activatePrintf);
	}
}

void combineReservoirs(inout Reservoir self, Reservoir other, float pHat[RESERVOIR_SIZE], uint randomSeed, bool activatePrintf) {
	self.numStreamSamples += other.numStreamSamples;
	if(activatePrintf)
	{
	debugPrintfEXT("combine reservoir %d \n", self.numStreamSamples);
	}
	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		float weight = pHat[i] * other.samples[i].w * other.numStreamSamples;
		if (weight > 0.0f) {
			updateReservoirAt(
				self, i, weight,
				other.samples[i].position_emissionLum.xyz, other.samples[i].normal, other.samples[i].position_emissionLum.w,
				other.samples[i].lightIndex, pHat[i],
				other.samples[i].w, randomSeed,
				activatePrintf
			);
		}
		if (self.samples[i].w > 0.0f) {
			// w?
			self.samples[i].w = self.samples[i].sumWeights / (self.numStreamSamples * self.samples[i].pHat);
		}
	}
}

Reservoir newReservoir() {
	Reservoir result;
	for (int i = 0; i < RESERVOIR_SIZE; ++i) {
		result.samples[i].sumWeights = 0.0f;
	}
	result.numStreamSamples = 0u;
	return result;
}
