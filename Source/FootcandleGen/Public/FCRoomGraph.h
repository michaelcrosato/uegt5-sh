#pragma once

#include "CoreMinimal.h"

// The room-and-portal graph (ROADMAP 5.2 stage 6, 7.2): rooms are nodes,
// apertures are edges. ONE graph serves navigation, sound propagation, and -
// at M4 - enemy hearing. Pure data, no UObjects: unit-testable and headless.
namespace FC::Gen
{
	// Aperture kinds carry their acoustic loss (ROADMAP 7.2 table). The
	// dynamic flag on a portal (door open/closed) picks which loss applies.
	enum class EAperture : uint8
	{
		OpenDoorway,     // 3
		DoorOpen,        // 5
		DoorClosedInterior, // 22
		DoorClosedExterior, // 30
		WindowOpen,      // 6
		WindowClosed,    // 25
		WindowBroken,    // 4
		Stairwell,       // 4
		FloorCeiling,    // 35
		Wall,            // 45
	};

	FOOTCANDLEGEN_API float ApertureLoss(EAperture Aperture);

	struct FRoom
	{
		int32 Id = INDEX_NONE;
		FVector Center = FVector::ZeroVector;
		// Axis-aligned bounds for point->room resolution.
		FVector BoundsMin = FVector::ZeroVector;
		FVector BoundsMax = FVector::ZeroVector;
	};

	struct FPortal
	{
		int32 Id = INDEX_NONE;
		int32 RoomA = INDEX_NONE;
		int32 RoomB = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		// Current acoustic state; doors flip this at runtime.
		EAperture State = EAperture::OpenDoorway;
	};

	struct FOOTCANDLEGEN_API FRoomGraph
	{
		TArray<FRoom> Rooms;
		TArray<FPortal> Portals;

		int32 AddRoom(const FVector& BoundsMin, const FVector& BoundsMax);
		int32 AddPortal(int32 RoomA, int32 RoomB, const FVector& Location, EAperture State);

		// Point -> containing room (INDEX_NONE if outside all bounds; the
		// caller treats that as the nearest room by center distance).
		int32 ResolveRoom(const FVector& Point) const;
		int32 ResolveRoomOrNearest(const FVector& Point) const;
	};

	// Propagated loudness per room for one noise event (ROADMAP 7.2):
	//   Loudness(node) = origin loudness
	//                  - distance attenuation along the graph path
	//                  - sum of aperture losses crossed
	// Dijkstra flood, cutoff below CutoffLoudness, hard cap MaxVisitedRooms.
	struct FPropagationResult
	{
		// Parallel to Graph.Rooms; <= 0 means inaudible in that room.
		TArray<float> LoudnessPerRoom;
	};

	FOOTCANDLEGEN_API FPropagationResult PropagateNoise(
		const FRoomGraph& Graph,
		int32 OriginRoom,
		const FVector& Origin,
		float Loudness,
		float CutoffLoudness = 1.0f,
		int32 MaxVisitedRooms = 40,
		float DistanceLossPerMeter = 0.55f);
}
