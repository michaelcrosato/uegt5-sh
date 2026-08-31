#include "Testing/FCM10SmokeSubsystem.h"

#include "Engine/World.h"
#include "Footcandle.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Player/FCPlayerCharacter.h"
#include "UnrealClient.h"

#if !UE_BUILD_SHIPPING

void UFCM10SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("fcm10smoke")))
	{
		return;
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCM10SMOKE] starting"));
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCM10SmokeSubsystem::Tick));
}

void UFCM10SmokeSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	Super::Deinitialize();
}

void UFCM10SmokeSubsystem::Check(const TCHAR* Name, const bool bCondition)
{
	if (bCondition)
	{
		++PassCount;
		UE_LOG(LogFootcandle, Display, TEXT("[FCM10SMOKE] PASS %s"), Name);
	}
	else
	{
		++FailCount;
		UE_LOG(LogFootcandle, Error, TEXT("[FCM10SMOKE] FAIL %s"), Name);
	}
}

bool UFCM10SmokeSubsystem::Tick(const float DeltaTime)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	if (Player == nullptr)
	{
		return true;
	}
	StepTime += DeltaTime;

	switch (Step)
	{
	case 0: // pause round-trip (single-player horror respects the doorbell)
		if (StepTime >= 2.0f)
		{
			PC->SetPause(true);
			Check(TEXT("Shell: pause pauses the world"), World->IsPaused());
			PC->SetPause(false);
			Check(TEXT("Shell: unpause resumes"), !World->IsPaused());

			// Death card: two strikes, then capture it.
			Player->ApplyHunterContact(TEXT("smoke"));
			Step = 1;
			StepTime = 0.0f;
		}
		break;

	case 1: // second strike after the grab cooldown
		if (StepTime >= 2.0f)
		{
			Player->ApplyHunterContact(TEXT("It heard the smoke test."));
			Check(TEXT("Shell: player dead for the card"),
				Player->GetHealthState() == EFCHealthState::Dead);
			Step = 2;
			StepTime = 0.0f;
		}
		break;

	case 2: // let the card render, then screenshot it
		if (StepTime >= 1.0f)
		{
			const FString Shot = FPaths::ProjectSavedDir() / TEXT("VisualCheck/FC_DeathCard.png");
			FScreenshotRequest::RequestScreenshot(Shot, false, false);
			Step = 3;
			StepTime = 0.0f;
		}
		break;

	case 3:
		if (StepTime >= 1.0f)
		{
			Check(TEXT("Shell: death card captured"),
				IFileManager::Get().FileSize(*(FPaths::ProjectSavedDir() / TEXT("VisualCheck/FC_DeathCard.png"))) > 0);
			Finish();
			return false;
		}
		break;

	default:
		break;
	}
	return true;
}

void UFCM10SmokeSubsystem::Finish()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCM10SMOKE] DONE pass=%d fail=%d"), PassCount, FailCount);
	FPlatformMisc::RequestExit(false);
}

#else

void UFCM10SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UFCM10SmokeSubsystem::Deinitialize() { Super::Deinitialize(); }
bool UFCM10SmokeSubsystem::Tick(float) { return false; }
void UFCM10SmokeSubsystem::Check(const TCHAR*, bool) {}
void UFCM10SmokeSubsystem::Finish() {}

#endif
