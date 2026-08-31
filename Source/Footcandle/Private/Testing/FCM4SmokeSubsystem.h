#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCM4SmokeSubsystem.generated.h"

// Scripted Watcher behavior smoke (M4 exit evidence, AI-01/02/07-flavored):
// hearing -> investigate near (not at) the noise; lit player in cone -> Hunt;
// dark still player -> no detection; contact -> Critical -> Dead with
// attribution. Run: -fcaddress -fcwatcher -fcm4smoke (as the player).
UCLASS()
class UFCM4SmokeSubsystem : public UWorldSubsystem
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
	float StepTime = 0.0f; // seconds in the current step (frame counts lie at 260fps)
	int32 Step = 0;
	int32 PassCount = 0;
	int32 FailCount = 0;
	bool bSawHunt = false;
	float PeakDarkMeter = 0.0f;
};
