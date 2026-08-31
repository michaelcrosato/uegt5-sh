#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCM8SmokeSubsystem.generated.h"

// The systemic run, end to end (M8): dark streets, two conditions (key +
// street power), Pressure rising per condition, the Director deploying the
// hunter, the rest guarantee holding, and extraction gated on BOTH.
// Run: -fccity=<seed> -fcrun -fcdirector -fcm8smoke (as the player).
UCLASS()
class UFCM8SmokeSubsystem : public UWorldSubsystem
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
