#include "FCBuildingGen.h"
#include "FCCityGen.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace FC::Gen;

namespace
{
	// Order-sensitive structural hash - any drift in generation output
	// changes it (the byte-comparison gate of ROADMAP 13.3, data level).
	uint64 HashBuilding(const FFCBuildingData& Building)
	{
		uint64 State = Building.Seed ^ (static_cast<uint64>(Building.BuildingId) << 32);
		auto Mix = [&State](const uint64 Value)
		{
			State ^= Value + 0x9E3779B97F4A7C15ull + (State << 6) + (State >> 2);
		};
		Mix(Building.FootprintCells.X); Mix(Building.FootprintCells.Y); Mix(Building.Floors);
		for (const FFCGenRoom& Room : Building.Rooms)
		{
			Mix(Room.Id); Mix(static_cast<uint64>(Room.Type)); Mix(Room.Floor);
			Mix(Room.CellMin.X); Mix(Room.CellMin.Y); Mix(Room.CellMax.X); Mix(Room.CellMax.Y);
		}
		for (const FFCGenPortal& Portal : Building.Portals)
		{
			Mix(Portal.Id); Mix(static_cast<uint64>(Portal.Kind));
			Mix(Portal.RoomA); Mix(Portal.RoomB);
			Mix(static_cast<uint64>(Portal.Position.X * 16.0));
			Mix(static_cast<uint64>(Portal.Position.Y * 16.0));
			Mix(static_cast<uint64>(Portal.Position.Z * 16.0));
		}
		for (const FFCGenProp& Prop : Building.Props)
		{
			Mix(static_cast<uint64>(Prop.Class)); Mix(Prop.Room);
			Mix(static_cast<uint64>(Prop.Position.X * 16.0));
			Mix(static_cast<uint64>(Prop.Position.Y * 16.0));
		}
		for (const FFCGenLight& Light : Building.Lights)
		{
			Mix(Light.Room); Mix(Light.Circuit);
		}
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBuildingDeterminismTest,
	"Footcandle.Gen.Building.Determinism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCBuildingDeterminismTest::RunTest(const FString& Parameters)
{
	const FFCBuildingRules Rules;
	for (uint64 Seed = 1; Seed <= 32; ++Seed)
	{
		const uint64 HashA = HashBuilding(GenerateBuilding(Seed * 7919ull, 3, Rules));
		const uint64 HashB = HashBuilding(GenerateBuilding(Seed * 7919ull, 3, Rules));
		TestEqual(TEXT("same seed -> identical building"), HashA, HashB);
		const uint64 HashC = HashBuilding(GenerateBuilding(Seed * 7919ull, 4, Rules));
		TestNotEqual(TEXT("different building id -> different building"), HashA, HashC);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCBuildingSoakTest,
	"Footcandle.Gen.Building.Soak1000",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCBuildingSoakTest::RunTest(const FString& Parameters)
{
	// The seed-soak pattern (ROADMAP 13.3): every failure names its seed and
	// joins the regression corpus. 1000 here per-build; the nightly headless
	// run scales the same loop to 10k.
	const FFCBuildingRules Rules;
	int32 Failures = 0;
	int32 TotalRooms = 0;
	int32 TotalPortals = 0;
	int32 Iterations = 0;
	for (uint64 Seed = 0; Seed < 1000; ++Seed)
	{
		++Iterations;
		const FFCBuildingData Building = GenerateBuilding(Seed * 0x9E3779B9ull + 17ull, 1, Rules);
		const TArray<FString> Problems = ValidateBuilding(Building);
		if (Problems.Num() > 0)
		{
			++Failures;
			AddError(FString::Printf(TEXT("seed %llu: %s"),
				Seed * 0x9E3779B9ull + 17ull, *FString::Join(Problems, TEXT("; "))));
			if (Failures > 5)
			{
				break; // enough evidence; don't drown the report
			}
		}
		TotalRooms += Building.Rooms.Num();
		TotalPortals += Building.Portals.Num();
	}
	AddInfo(FString::Printf(TEXT("%d seeds: avg rooms=%.1f avg portals=%.1f failures=%d"),
		Iterations, TotalRooms / static_cast<float>(Iterations),
		TotalPortals / static_cast<float>(Iterations), Failures));
	return Failures == 0;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCCitySoakTest,
	"Footcandle.Gen.City.Soak100",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCCitySoakTest::RunTest(const FString& Parameters)
{
	int32 Failures = 0;
	for (uint64 Seed = 0; Seed < 100; ++Seed)
	{
		const FFCCityData City = GenerateCity(Seed * 104729ull + 5ull, FFCCityRules());
		const TArray<FString> Problems = ValidateCity(City);
		if (Problems.Num() > 0)
		{
			++Failures;
			AddError(FString::Printf(TEXT("city seed %llu: %s"),
				Seed * 104729ull + 5ull, *FString::Join(Problems, TEXT("; "))));
			if (Failures > 3)
			{
				break;
			}
		}
		// Lots must not overlap: footprints stay inside their lot pitch by
		// construction; verify the invariant anyway (guards future jitter).
		for (const FFCCityLot& Lot : City.Lots)
		{
			const float FootW = Lot.Building.FootprintCells.X * CellSize;
			const float FootD = Lot.Building.FootprintCells.Y * CellSize;
			const float LotX = (Lot.LotId % City.LotGrid.X) * LotPitch;
			const float LotY = (Lot.LotId / City.LotGrid.X) * LotPitch;
			if (Lot.Origin.X < LotX - 0.5f || Lot.Origin.Y < LotY - 0.5f
				|| Lot.Origin.X + FootW > LotX + LotPitch - StreetWidth + 0.5f
				|| Lot.Origin.Y + FootD > LotY + LotPitch - StreetWidth + 0.5f)
			{
				AddError(FString::Printf(TEXT("lot %d escapes its bounds"), Lot.LotId));
				++Failures;
			}
		}
	}
	return Failures == 0;
}

#endif // WITH_DEV_AUTOMATION_TESTS
