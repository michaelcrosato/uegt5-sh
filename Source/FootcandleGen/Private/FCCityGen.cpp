#include "FCCityGen.h"

#include "FCGenSeed.h"
#include "FCGenStageIds.h"

namespace FC::Gen
{
	FFCCityData GenerateCity(const uint64 Seed, const FFCCityRules& Rules)
	{
		FFCCityData City;
		City.Seed = Seed;
		City.LotGrid = Rules.LotGrid;

		for (int32 LotY = 0; LotY < Rules.LotGrid.Y; ++LotY)
		{
			for (int32 LotX = 0; LotX < Rules.LotGrid.X; ++LotX)
			{
				const int32 LotId = LotY * Rules.LotGrid.X + LotX;
				FFCCityLot& Lot = City.Lots.AddDefaulted_GetRef();
				Lot.LotId = LotId;
				Lot.Building = GenerateBuilding(Seed, /*BuildingId*/ LotId + 1, Rules.BuildingRules);

				// Center the footprint in its lot; jitter from the Blocks
				// stage stream so identical footprints don't align in rows.
				FRandomStream Stream = MakeStream(Seed, StageId(EStage::Blocks), LotId);
				const float FootW = Lot.Building.FootprintCells.X * CellSize;
				const float FootD = Lot.Building.FootprintCells.Y * CellSize;
				const float SlackX = (LotPitch - StreetWidth) - FootW;
				const float SlackY = (LotPitch - StreetWidth) - FootD;
				Lot.Origin = FVector(
					LotX * LotPitch + Stream.FRandRange(0.0f, FMath::Max(SlackX, 0.0f)),
					LotY * LotPitch + Stream.FRandRange(0.0f, FMath::Max(SlackY, 0.0f)),
					0.0f);
			}
		}

		// Streetlights: one per lot along each horizontal street south of the
		// lot row (light-density authorship, 6.2 - streets are lit by default;
		// darkness is the grid's decision later).
		for (int32 LotY = 0; LotY <= Rules.LotGrid.Y; ++LotY)
		{
			for (int32 LotX = 0; LotX < Rules.LotGrid.X; ++LotX)
			{
				City.StreetLightPositions.Add(FVector(
					LotX * LotPitch + (LotPitch - StreetWidth) * 0.5f,
					LotY * LotPitch - StreetWidth * 0.5f,
					550.0f));
			}
		}

		City.ExtentMin = FVector2D(-StreetWidth, -StreetWidth);
		City.ExtentMax = FVector2D(
			Rules.LotGrid.X * LotPitch,
			Rules.LotGrid.Y * LotPitch);
		return City;
	}

	TArray<FString> ValidateCity(const FFCCityData& City)
	{
		TArray<FString> Failures;
		for (const FFCCityLot& Lot : City.Lots)
		{
			TArray<FString> BuildingProblems = ValidateBuilding(Lot.Building);
			for (const FString& Problem : BuildingProblems)
			{
				Failures.Add(FString::Printf(TEXT("lot %d: %s"), Lot.LotId, *Problem));
			}
		}
		if (City.StreetLightPositions.Num() < City.Lots.Num())
		{
			Failures.Add(TEXT("LGT: street light floor not met"));
		}
		return Failures;
	}
}
