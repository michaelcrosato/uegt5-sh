#include "FCGenSeed.h"
#include "FCGenStageIds.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// GOLDEN VECTORS - computed once when the algorithm was frozen (2026-08-31)
// and asserted forever. If this test fails, generation output has changed and
// every existing seed and save is corrupt: fix the regression, never the
// constants. (docs/ROADMAP.md §5.1, risk R2.)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCGenSeedGoldenTest,
	"Footcandle.Gen.Seed.Golden",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCGenSeedGoldenTest::RunTest(const FString& Parameters)
{
	using namespace FC::Gen;

	TestEqual(TEXT("Derive(0,0,0)"),
		DeriveSeed(0ull, 0u, 0ull), 0x811F12291400C836ull);
	TestEqual(TEXT("Derive(0xDEADBEEF,1,0)"),
		DeriveSeed(0xDEADBEEFull, 1u, 0ull), 0x30048160289FCDA0ull);
	TestEqual(TEXT("Derive(0xDEADBEEF,1,1)"),
		DeriveSeed(0xDEADBEEFull, 1u, 1ull), 0xC235EB5A526269B2ull);
	TestEqual(TEXT("Derive(0xDEADBEEF,2,1)"),
		DeriveSeed(0xDEADBEEFull, 2u, 1ull), 0x4CC0959A60D2FC8Cull);
	TestEqual(TEXT("Derive(123456789,7,42)"),
		DeriveSeed(123456789ull, 7u, 42ull), 0xD0FF61A24E6A8816ull);
	TestEqual(TEXT("Derive(max,max,max)"),
		DeriveSeed(0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFu, 0xFFFFFFFFFFFFFFFFull), 0xB670C0BC8F7EDFFCull);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCGenSeedPropertiesTest,
	"Footcandle.Gen.Seed.Properties",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCGenSeedPropertiesTest::RunTest(const FString& Parameters)
{
	using namespace FC::Gen;

	// Repeatability: identical inputs, identical outputs.
	for (uint64 Seed = 0; Seed < 64; ++Seed)
	{
		TestEqual(TEXT("Derive repeatable"),
			DeriveSeed(Seed, 3u, Seed * 7u), DeriveSeed(Seed, 3u, Seed * 7u));
	}

	// Sensitivity: each input axis changes the output.
	const uint64 Base = DeriveSeed(42ull, StageId(EStage::Massing), 5ull);
	TestNotEqual(TEXT("GlobalSeed matters"), Base, DeriveSeed(43ull, StageId(EStage::Massing), 5ull));
	TestNotEqual(TEXT("StageId matters"), Base, DeriveSeed(42ull, StageId(EStage::FloorPlans), 5ull));
	TestNotEqual(TEXT("Cell matters"), Base, DeriveSeed(42ull, StageId(EStage::Massing), 6ull));

	// Streams from the same derivation produce identical sequences.
	FRandomStream StreamA = MakeStream(7ull, StageId(EStage::Roads), 11ull);
	FRandomStream StreamB = MakeStream(7ull, StageId(EStage::Roads), 11ull);
	for (int32 Draw = 0; Draw < 32; ++Draw)
	{
		TestEqual(TEXT("Stream sequence identical"), StreamA.RandRange(0, 1000000), StreamB.RandRange(0, 1000000));
	}

	// No trivial collisions across a small grid of cells within one stage.
	TSet<uint64> Seen;
	for (uint64 Cell = 0; Cell < 4096; ++Cell)
	{
		Seen.Add(DeriveSeed(99ull, StageId(EStage::Population), Cell));
	}
	TestEqual(TEXT("4096 cells -> 4096 distinct sub-seeds"), Seen.Num(), 4096);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
