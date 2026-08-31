#include "Testing/FCM7SmokeSubsystem.h"

#include "Engine/World.h"
#include "Footcandle.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Player/FCPlayerCharacter.h"
#include "Testing/FCCitySubsystem.h"

#if !UE_BUILD_SHIPPING

void UFCM7SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("fcm7smoke")))
	{
		return;
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCM7SMOKE] starting"));
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCM7SmokeSubsystem::Tick));
}

void UFCM7SmokeSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	Super::Deinitialize();
}

void UFCM7SmokeSubsystem::Check(const TCHAR* Name, const bool bCondition)
{
	if (bCondition)
	{
		++PassCount;
		UE_LOG(LogFootcandle, Display, TEXT("[FCM7SMOKE] PASS %s"), Name);
	}
	else
	{
		++FailCount;
		UE_LOG(LogFootcandle, Error, TEXT("[FCM7SMOKE] FAIL %s"), Name);
	}
}

bool UFCM7SmokeSubsystem::Tick(const float DeltaTime)
{
	using namespace FC::Gen;
	UWorld* World = GetWorld();
	UFCCitySubsystem* CitySub = World->GetSubsystem<UFCCitySubsystem>();
	const APlayerController* PC = World->GetFirstPlayerController();
	AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	if (CitySub == nullptr || Player == nullptr)
	{
		return true;
	}
	StepTime += DeltaTime;
	const FFCCityData& City = CitySub->GetCityData();

	switch (Step)
	{
	case 0: // all shells up; a nearby lot streamed detail
		if (StepTime >= 3.0f)
		{
			Check(TEXT("City: every lot has a shell"),
				City.Lots.Num() > 0 && CitySub->CountLotsWithShell() == City.Lots.Num());
			FirstDetailLot = CitySub->GetDetailLot();
			Check(TEXT("City: nearest lot streamed interior detail"), FirstDetailLot != INDEX_NONE);

			// Walk (teleport) to the far corner lot's doorstep.
			const FFCCityLot& Far = City.Lots.Last();
			Player->TeleportTo(Far.Origin + FVector(
				Far.Building.FootprintCells.X * CellSize * 0.5f, -300, 120),
				FRotator(0, 90, 0), false, true);
			Step = 1;
			StepTime = 0.0f;
		}
		break;

	case 1: // the streamer follows: detail moves to the far lot
		if (StepTime >= 2.5f)
		{
			const int32 NowLot = CitySub->GetDetailLot();
			Check(TEXT("City: detail streamed to the new nearest lot"),
				NowLot != INDEX_NONE && NowLot != FirstDetailLot
				&& NowLot == City.Lots.Last().LotId);
			Finish();
			return false;
		}
		break;

	default:
		break;
	}
	return true;
}

void UFCM7SmokeSubsystem::Finish()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCM7SMOKE] DONE pass=%d fail=%d"), PassCount, FailCount);
	FPlatformMisc::RequestExit(false);
}

#else

void UFCM7SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UFCM7SmokeSubsystem::Deinitialize() { Super::Deinitialize(); }
bool UFCM7SmokeSubsystem::Tick(float) { return false; }
void UFCM7SmokeSubsystem::Check(const TCHAR*, bool) {}
void UFCM7SmokeSubsystem::Finish() {}

#endif
