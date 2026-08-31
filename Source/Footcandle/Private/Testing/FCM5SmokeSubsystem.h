#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCM5SmokeSubsystem.generated.h"

// Generated-building smoke (M5): the building spawned from data is playable
// and honest - doors bound to portals, propagation muffled by generated
// topology, and save v1 round-trips doors + player through the delta log.
// Run: -fcgenbuilding=<seed> -fcm5smoke (as the player).
UCLASS()
class UFCM5SmokeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float DeltaTime);
	void Check(const TCHAR* Name, bool bCondition);
	void Finish();

	FTSTicker::FDelegateHandle TickerHandle;
	float StepTime = 0.0f;
	int32 Step = 0;
	int32 PassCount = 0;
	int32 FailCount = 0;
	FVector SavedSpot = FVector::ZeroVector;
};
