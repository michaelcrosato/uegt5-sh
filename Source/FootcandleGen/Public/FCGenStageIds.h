#pragma once

#include "CoreMinimal.h"

namespace FC::Gen
{
	// Generation pipeline stage ids (docs/ROADMAP.md §5.2).
	// FROZEN numbering - these feed DeriveSeed, so renumbering regenerates
	// every city ever seeded. Append new stages; never reorder or reuse.
	enum class EStage : uint32
	{
		Terrain = 1,
		Districts = 2,
		Roads = 3,
		Blocks = 4,
		Massing = 5,
		FloorPlans = 6,
		Apertures = 7,
		PowerAndLighting = 8,
		Population = 9,
		Objectives = 10,

		// Non-city streams (still seed-derived for determinism):
		DevScene = 1000,
	};

	FORCEINLINE uint32 StageId(const EStage Stage)
	{
		return static_cast<uint32>(Stage);
	}
}
