#pragma once

#include "CoreMinimal.h"
#include "FCRoomGraph.h"

// Single-building procedural generation (ROADMAP 5.2 stages 5-9, building
// scope; the city-scale stages land at M7). Pure data out, deterministic by
// construction, validated before anything spawns (ROADMAP 5.6).
//
// Grid (ROADMAP 5.3, frozen): cell 400 cm, wall 20 cm, floor-to-floor
// 320 cm, door 100x210, window 150x130 sill 90.
namespace FC::Gen
{
	constexpr float CellSize = 400.0f;
	constexpr float WallThickness = 20.0f;
	constexpr float FloorHeight = 320.0f;

	enum class ERoomType : uint8
	{
		Entry,      // ground-floor room with the exterior door
		Room,       // generic interior
		Stairwell,  // vertical connector
	};

	enum class EPropClass : uint8
	{
		NoiseSmall,   // bottle/can - loud, light
		NoiseMedium,  // chair/box - loud, heavier
		Hideable,     // locker - the hide verb
	};

	struct FFCGenRoom
	{
		int32 Id = INDEX_NONE;
		int32 Floor = 0;
		ERoomType Type = ERoomType::Room;
		// Cell-space rect (inclusive min, exclusive max).
		FIntPoint CellMin = FIntPoint::ZeroValue;
		FIntPoint CellMax = FIntPoint::ZeroValue;
	};

	enum class EGenPortalKind : uint8
	{
		ExteriorDoor,
		InteriorDoor,
		Window,
		StairOpening,
	};

	struct FFCGenPortal
	{
		int32 Id = INDEX_NONE;
		EGenPortalKind Kind = EGenPortalKind::InteriorDoor;
		int32 RoomA = INDEX_NONE;         // INDEX_NONE = exterior
		int32 RoomB = INDEX_NONE;
		FVector Position = FVector::ZeroVector; // world cm, portal center on wall
		bool bAlongX = true;              // wall orientation the portal sits in
	};

	struct FFCGenProp
	{
		EPropClass Class = EPropClass::NoiseSmall;
		int32 Room = INDEX_NONE;
		FVector Position = FVector::ZeroVector;
	};

	struct FFCGenLight
	{
		int32 Room = INDEX_NONE;
		int32 Circuit = INDEX_NONE;       // per-floor circuits (ROADMAP 8.5 seam)
		FVector Position = FVector::ZeroVector;
	};

	struct FFCBuildingData
	{
		uint64 Seed = 0;
		int32 BuildingId = 0;
		FIntPoint FootprintCells = FIntPoint(3, 2);
		int32 Floors = 2;
		TArray<FFCGenRoom> Rooms;
		TArray<FFCGenPortal> Portals;
		TArray<FFCGenProp> Props;
		TArray<FFCGenLight> Lights;
		int32 CircuitCount = 0;
	};

	struct FFCBuildingRules
	{
		FIntPoint MinFootprint = FIntPoint(2, 2);
		FIntPoint MaxFootprint = FIntPoint(4, 3);
		int32 MinFloors = 2;
		int32 MaxFloors = 3;
	};

	// The pure function (P7): same (Seed, BuildingId, Rules) -> same building.
	FOOTCANDLEGEN_API FFCBuildingData GenerateBuilding(
		uint64 GlobalSeed, int32 BuildingId, const FFCBuildingRules& Rules);

	// Data-level validation (ROADMAP 5.6 checks 1-8, building scope).
	// Returns failure reason codes; empty = valid.
	FOOTCANDLEGEN_API TArray<FString> ValidateBuilding(const FFCBuildingData& Building);

	// The acoustic room graph for this building (rooms + street cell last),
	// shared by sound and - later - the hunters (one graph, ROADMAP 5.2).
	FOOTCANDLEGEN_API FRoomGraph BuildRoomGraph(const FFCBuildingData& Building,
		const FVector& WorldOrigin, TArray<int32>& OutPortalIdByGenPortal);
}
