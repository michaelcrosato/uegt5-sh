#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "FCCityGen.h"
#include "Subsystems/WorldSubsystem.h"
#include "World/FCBuildingSpawner.h"
#include "FCCitySubsystem.generated.h"

// The city, first pass (M7 core): -fccity=<seed> generates and validates a
// lot-grid district, spawns every building's SHELL, lights the streets, and
// streams DETAIL (interiors, doors, lights, props) for the building nearest
// the player - swapping the acoustic room graph with it (ROADMAP 5.5's
// tiers: data always / shell in range / interior on approach).
UCLASS()
class UFCCitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	bool SpawnFromSeed(uint64 Seed);

	const FC::Gen::FFCCityData& GetCityData() const { return City; }
	int32 GetDetailLot() const { return DetailLot; }
	int32 CountLotsWithShell() const;

	float DetailRadius = 2600.0f;

private:
	bool TickStreamer(float DeltaTime);

	FC::Gen::FFCCityData City;
	TArray<FFCSpawnedBuilding> SpawnedLots;
	int32 DetailLot = INDEX_NONE;
	FTSTicker::FDelegateHandle StreamTicker;
	bool bSpawned = false;
};
