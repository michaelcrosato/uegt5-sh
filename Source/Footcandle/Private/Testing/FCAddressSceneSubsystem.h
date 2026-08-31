#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCAddressSceneSubsystem.generated.h"

class AStaticMeshActor;

// "One Address" (M1): a hand-authored-in-code two-floor building - walls with
// door and window openings, blocky stairs, per-room lights with a working
// switch, physics noise props, a hide-in locker, an amber streetlight
// outside. The M1 playground and the M2/M3 lighting + audio test bed.
//
// Trigger: -fcaddress on the command line, or `fc.Address.Spawn`.
UCLASS()
class UFCAddressSceneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void SpawnScene();

private:
	AStaticMeshActor* SpawnBox(const FVector& MinCorner, const FVector& MaxCorner);

	// Axis-aligned wall with door/window openings cut along its length.
	// bAlongX: wall runs along X at fixed Y (thickness in Y); else along Y.
	struct FOpening
	{
		float Start = 0.0f;   // along the wall axis, cm
		float End = 0.0f;
		float BottomZ = 0.0f; // opening vertical span
		float TopZ = 0.0f;
	};
	void BuildWall(bool bAlongX, float FixedAxisPos, float AxisStart, float AxisEnd,
		float Z0, float Z1, float Thickness, const TArray<FOpening>& Openings);

	bool bSpawned = false;
};
