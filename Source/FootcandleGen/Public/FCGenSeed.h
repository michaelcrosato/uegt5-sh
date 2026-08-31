#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

namespace FC::Gen
{
	// Seed derivation for all procedural generation (docs/ROADMAP.md §5.1).
	//
	// FROZEN 2026-08-31. These functions are a compatibility contract: every
	// generated city, every save file, and the golden tests in FootcandleTests
	// depend on their exact bit-for-bit output. Changing them corrupts every
	// existing seed. A better scheme means a NEW function name and a migration,
	// never an edit here.

	// splitmix64 step: advances State and returns the next 64-bit value.
	FOOTCANDLEGEN_API uint64 MixSplitMix64(uint64& State);

	// Derives the sub-seed for (GlobalSeed, StageId, Cell). StageIds are in
	// FCGenStageIds.h; Cell disambiguates per-entity streams within a stage
	// (block index, building id, room id - caller-defined, but stable).
	FOOTCANDLEGEN_API uint64 DeriveSeed(uint64 GlobalSeed, uint32 StageId, uint64 Cell = 0);

	// FRandomStream seeded from the high 32 bits of DeriveSeed.
	// Rule (docs/ROADMAP.md §5.1): this is the ONLY way generation code may
	// obtain randomness. Never FMath::Rand, never a shared stream.
	FOOTCANDLEGEN_API FRandomStream MakeStream(uint64 GlobalSeed, uint32 StageId, uint64 Cell = 0);
}
