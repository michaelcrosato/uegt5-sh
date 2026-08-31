#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCM10SmokeSubsystem.generated.h"

// Shell smoke (M10 core): pause actually pauses, and the death card renders
// (captured for the eyeball pass - the seed must be on it).
// Run: -fccity=<seed> -fcrun -fcm10smoke (as the player).
UCLASS()
class UFCM10SmokeSubsystem : public UWorldSubsystem
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
};
