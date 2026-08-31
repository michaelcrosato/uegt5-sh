#pragma once

#include "CoreMinimal.h"
#include "FCBuildingGen.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCGenBuildingSubsystem.generated.h"

class AFCDoor;

// Spawns a GENERATED building (M5): -fcgenbuilding=<seed> generates,
// validates (refusing invalid data - ROADMAP 5.6), then builds walls with
// carved openings, slabs with stair holes, blocky stairs, doors bound to
// their acoustic portals, registered lights, props, hideables, and the room
// graph - the same playable contract as the hand-built address, from data.
UCLASS()
class UFCGenBuildingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	bool SpawnFromSeed(uint64 Seed);

	const FC::Gen::FFCBuildingData& GetBuildingData() const { return Building; }
	const TArray<TObjectPtr<AFCDoor>>& GetDoors() const { return Doors; }

private:
	class AStaticMeshActor* SpawnBox(const FVector& MinCorner, const FVector& MaxCorner);
	void EmitWallRun(bool bAlongX, float FixedPos, float RunStart, float RunEnd,
		float Z0, float Z1, int32 Floor);

	FC::Gen::FFCBuildingData Building;
	TArray<TObjectPtr<AFCDoor>> Doors;
	bool bSpawned = false;
};
