#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCM1SmokeSubsystem.generated.h"

// Scripted behavioral smoke for the M1 slice: drives the real player through
// move / sprint+stamina / footstep-noise / door / switch / hide / vault in a
// live -game session and logs [FCM1SMOKE] PASS/FAIL lines for the script to
// grep. Run: -fcaddress -fcm1smoke (as the player character, not spectator).
UCLASS()
class UFCM1SmokeSubsystem : public UWorldSubsystem
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
	int32 Frame = 0;
	int32 Step = 0;
	int32 PassCount = 0;
	int32 FailCount = 0;
	int32 NoiseEventCount = 0;
	int32 FootstepNoiseCount = 0;
	FVector MoveStart = FVector::ZeroVector;
	float StaminaBefore = 0.0f;
	FVector HideEntryFrom = FVector::ZeroVector;
	FDelegateHandle NoiseHandle;
};
