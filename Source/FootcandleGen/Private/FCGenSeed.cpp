#include "FCGenSeed.h"

namespace FC::Gen
{
	uint64 MixSplitMix64(uint64& State)
	{
		State += 0x9E3779B97F4A7C15ull;
		uint64 Z = State;
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		return Z ^ (Z >> 31);
	}

	uint64 DeriveSeed(const uint64 GlobalSeed, const uint32 StageId, const uint64 Cell)
	{
		uint64 State = GlobalSeed ^ 0xF00D5EEDCAFEF00Dull;
		MixSplitMix64(State);
		State ^= (static_cast<uint64>(StageId) << 1) | 1ull;
		const uint64 A = MixSplitMix64(State);
		State ^= Cell + 0x9E3779B97F4A7C15ull;
		const uint64 B = MixSplitMix64(State);
		return A ^ B;
	}

	FRandomStream MakeStream(const uint64 GlobalSeed, const uint32 StageId, const uint64 Cell)
	{
		const uint64 Derived = DeriveSeed(GlobalSeed, StageId, Cell);
		return FRandomStream(static_cast<int32>(Derived >> 32));
	}
}
