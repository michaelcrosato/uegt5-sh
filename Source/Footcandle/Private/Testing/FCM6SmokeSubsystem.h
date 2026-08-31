#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCM6SmokeSubsystem.generated.h"

// The vertical-slice loop, end to end (M6 mechanical evidence): in a
// GENERATED building with a LIVE Watcher, take the key upstairs, commit the
// extraction, survive the window, win - and the commit noise must pull the
// Watcher in. Run: -fcgenbuilding=<seed> -fcslice -fcwatcher -fcm6smoke.
// (The human halves of the M6 gate - external playtests, feel - remain the
// director's; this is the machine's half.)
UCLASS()
class UFCM6SmokeSubsystem : public UWorldSubsystem
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
	float WatcherDistAtCommit = 0.0f;
};
