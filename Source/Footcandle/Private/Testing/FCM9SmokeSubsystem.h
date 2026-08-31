#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCM9SmokeSubsystem.generated.h"

// The Listener + weather smoke (M9 core): freezes in silence, moves on
// noise, investigates precisely; rain (ambient floor) masks quiet sounds
// AND unlocks its mobility - the double-edged weather (ROADMAP 8.2).
// Run: -fccity=<seed> -fclistener -fcm9smoke (as the player).
UCLASS()
class UFCM9SmokeSubsystem : public UWorldSubsystem
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
	FVector FrozenPos = FVector::ZeroVector;
	FVector RainStartPos = FVector::ZeroVector;
};
