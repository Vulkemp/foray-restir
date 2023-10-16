
struct Reservoir
{
	uint y; // lightIndex
	float Wsum; // sum of weights
	uint M; // num samples seen so far
};

void updateReservoir( inout Reservoir r, uint xi, float wi, inout uint seed)
{
	r.Wsum += wi; // sum of all weights increases
	r.M += 1; // sum of samples seen so far increases
	if( rand(seed) < ( wi / r.Wsum ) )
	{
		r.y = xi;
	}
}