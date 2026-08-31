#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCM7SmokeSubsystem.generated.h"

// City streaming smoke (M7 core): all lots carry shells; exactly the lot
// nearest the player carries interior detail; crossing the city moves the
// detail (old interiors despawn, new spawn, room graph follows).
// Run: -fccity=<seed> -fcm7smoke (as the player).
UCLASS()
class UFCM7SmokeSubsystem : public UWorldSubsystem
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
	int32 FirstDetailLot = INDEX_NONE;
};
