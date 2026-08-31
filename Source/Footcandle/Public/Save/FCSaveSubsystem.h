#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FCSaveSubsystem.generated.h"

// Save v1 (ROADMAP 12, scheduled at M5 by review finding #2): the save IS
// the seed plus a delta log - kilobytes, not world dumps. Deltas key off
// stable generation ids (door portal ids), never actor pointers.
UCLASS()
class UFCSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	uint64 Seed = 0;

	UPROPERTY()
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator PlayerRotation = FRotator::ZeroRotator;

	UPROPERTY()
	float Battery = 100.0f;

	UPROPERTY()
	float Stamina = 100.0f;

	// Door open-ness by stable door index (spawn order = generation order).
	UPROPERTY()
	TArray<bool> DoorOpen;
};

UCLASS()
class FOOTCANDLE_API UFCSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// Console: fc.Save / fc.Load (slot "FCDev").
	bool SaveNow(const FString& Slot);
	bool LoadNow(const FString& Slot);
};
