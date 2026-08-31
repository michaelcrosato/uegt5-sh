#include "FCRoomGraph.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace FC::Gen;

namespace
{
	// The canonical 3-room test house (ROADMAP 13.2 AUD-02 shape):
	// [Street] --entry door-- [Hall] --interior door-- [Bedroom]
	// 10m rooms, portals at the shared walls.
	FRoomGraph MakeThreeRooms(const EAperture EntryState, const EAperture InteriorState)
	{
		FRoomGraph Graph;
		const int32 Street = Graph.AddRoom(FVector(-1000, 0, 0), FVector(0, 1000, 300));
		const int32 Hall = Graph.AddRoom(FVector(0, 0, 0), FVector(1000, 1000, 300));
		const int32 Bedroom = Graph.AddRoom(FVector(1000, 0, 0), FVector(2000, 1000, 300));
		Graph.AddPortal(Street, Hall, FVector(0, 500, 100), EntryState);
		Graph.AddPortal(Hall, Bedroom, FVector(1000, 500, 100), InteriorState);
		return Graph;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFCNoisePropagationTest,
	"Footcandle.Noise.Propagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFCNoisePropagationTest::RunTest(const FString& Parameters)
{
	const FVector HallCenter(500, 500, 150);

	// 1. Open doors: sound crosses both portals, attenuating by distance+loss.
	{
		FRoomGraph Graph = MakeThreeRooms(EAperture::DoorOpen, EAperture::DoorOpen);
		const FPropagationResult Result = PropagateNoise(Graph, 1, HallCenter, 60.0f);
		TestEqual(TEXT("origin room keeps full loudness"), Result.LoudnessPerRoom[1], 60.0f);
		// Hall center -> portal = 5m -> 2.75 distance + 5 door = arriving 52.25.
		TestTrue(TEXT("street hears through open door"), Result.LoudnessPerRoom[0] > 45.0f);
		TestTrue(TEXT("bedroom hears through open door"), Result.LoudnessPerRoom[2] > 45.0f);
	}

	// 2. Closing the interior door drops the bedroom by ~17 (22 vs 5 loss).
	{
		FRoomGraph OpenGraph = MakeThreeRooms(EAperture::DoorOpen, EAperture::DoorOpen);
		FRoomGraph ClosedGraph = MakeThreeRooms(EAperture::DoorOpen, EAperture::DoorClosedInterior);
		const float OpenLoud = PropagateNoise(OpenGraph, 1, HallCenter, 60.0f).LoudnessPerRoom[2];
		const float ClosedLoud = PropagateNoise(ClosedGraph, 1, HallCenter, 60.0f).LoudnessPerRoom[2];
		TestEqual(TEXT("closed interior door costs exactly the table delta (17)"),
			OpenLoud - ClosedLoud, 17.0f, 0.01f);
	}

	// 3. A quiet noise dies at a closed exterior door entirely.
	{
		FRoomGraph Graph = MakeThreeRooms(EAperture::DoorClosedExterior, EAperture::DoorOpen);
		const FPropagationResult Result = PropagateNoise(Graph, 1, HallCenter, 25.0f);
		TestEqual(TEXT("sneak-level noise inaudible through closed exterior door"),
			Result.LoudnessPerRoom[0], 0.0f);
	}

	// 4. Origin position matters: noise beside the portal leaks louder than
	//    noise in the far corner.
	{
		FRoomGraph Graph = MakeThreeRooms(EAperture::DoorOpen, EAperture::DoorOpen);
		const float NearPortal = PropagateNoise(Graph, 1, FVector(50, 500, 150), 60.0f).LoudnessPerRoom[0];
		const float FarCorner = PropagateNoise(Graph, 1, FVector(950, 950, 150), 60.0f).LoudnessPerRoom[0];
		TestTrue(TEXT("closer to the door leaks louder"), NearPortal > FarCorner + 3.0f);
	}

	// 5. Determinism: identical inputs, identical results.
	{
		FRoomGraph Graph = MakeThreeRooms(EAperture::DoorOpen, EAperture::DoorClosedInterior);
		const FPropagationResult A = PropagateNoise(Graph, 1, HallCenter, 60.0f);
		const FPropagationResult B = PropagateNoise(Graph, 1, HallCenter, 60.0f);
		for (int32 Room = 0; Room < A.LoudnessPerRoom.Num(); ++Room)
		{
			TestEqual(TEXT("deterministic propagation"), A.LoudnessPerRoom[Room], B.LoudnessPerRoom[Room]);
		}
	}

	// 6. Visit cap: a long chain terminates without runaway.
	{
		FRoomGraph Chain;
		int32 Previous = Chain.AddRoom(FVector(0, 0, 0), FVector(100, 100, 100));
		for (int32 Index = 1; Index < 100; ++Index)
		{
			const int32 Next = Chain.AddRoom(
				FVector(Index * 100.0f, 0, 0), FVector(Index * 100.0f + 100.0f, 100, 100));
			Chain.AddPortal(Previous, Next, FVector(Index * 100.0f, 50, 50), EAperture::OpenDoorway);
			Previous = Next;
		}
		const FPropagationResult Result = PropagateNoise(Chain, 0, FVector(50, 50, 50), 10000.0f,
			1.0f, /*MaxVisitedRooms*/ 40);
		int32 AudibleRooms = 0;
		for (const float RoomLoudness : Result.LoudnessPerRoom)
		{
			if (RoomLoudness > 0.0f)
			{
				++AudibleRooms;
			}
		}
		TestTrue(TEXT("visit cap bounds the flood"), AudibleRooms <= 41);
	}

	// 7. Room resolution: inside, outside->nearest.
	{
		const FRoomGraph Graph = MakeThreeRooms(EAperture::DoorOpen, EAperture::DoorOpen);
		TestEqual(TEXT("point resolves to hall"), Graph.ResolveRoom(FVector(500, 500, 100)), 1);
		TestEqual(TEXT("outside point resolves to nearest"),
			Graph.ResolveRoomOrNearest(FVector(2500, 500, 100)), 2);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
