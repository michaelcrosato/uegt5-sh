#pragma once

#include "CoreMinimal.h"
#include "FCBuildingGen.h"

// City-scale generation, first pass (ROADMAP 5.2 stages 1-4 as a street
// grid; road-graph organics arrive with district work). A city is lots on
// a block grid separated by streets, each lot running the building
// generator with its own stable id - so every building soak result carries
// straight over.
namespace FC::Gen
{
	constexpr float StreetWidth = 1200.0f;
	constexpr float LotPitch = 4 * CellSize + StreetWidth; // max footprint + street

	struct FFCCityLot
	{
		int32 LotId = 0;
		FVector Origin = FVector::ZeroVector; // building min-corner, world cm
		FFCBuildingData Building;
	};

	struct FFCCityData
	{
		uint64 Seed = 0;
		FIntPoint LotGrid = FIntPoint(3, 2);
		TArray<FFCCityLot> Lots;
		TArray<FVector> StreetLightPositions;
		FVector2D ExtentMin = FVector2D::ZeroVector;
		FVector2D ExtentMax = FVector2D::ZeroVector;
	};

	struct FFCCityRules
	{
		FIntPoint LotGrid = FIntPoint(3, 2);
		FFCBuildingRules BuildingRules;
	};

	FOOTCANDLEGEN_API FFCCityData GenerateCity(uint64 Seed, const FFCCityRules& Rules);

	// Aggregate validation: every lot's building must validate; streets carry
	// at least one light per block edge (the light-density floor, 6.2).
	FOOTCANDLEGEN_API TArray<FString> ValidateCity(const FFCCityData& City);
}
