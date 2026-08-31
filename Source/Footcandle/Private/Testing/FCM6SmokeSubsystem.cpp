#include "Testing/FCM6SmokeSubsystem.h"

#include "AI/FCWatcher.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Footcandle.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Objectives/FCExtractZone.h"
#include "Objectives/FCKeyItem.h"
#include "Objectives/FCRunSubsystem.h"
#include "Player/FCPlayerCharacter.h"

#if !UE_BUILD_SHIPPING

namespace
{
	template <typename T>
	T* FirstActor(UWorld* World)
	{
		for (TActorIterator<T> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

void UFCM6SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("fcm6smoke")))
	{
		return;
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCM6SMOKE] starting"));
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCM6SmokeSubsystem::Tick));
}

void UFCM6SmokeSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	Super::Deinitialize();
}

void UFCM6SmokeSubsystem::Check(const TCHAR* Name, const bool bCondition)
{
	if (bCondition)
	{
		++PassCount;
		UE_LOG(LogFootcandle, Display, TEXT("[FCM6SMOKE] PASS %s"), Name);
	}
	else
	{
		++FailCount;
		UE_LOG(LogFootcandle, Error, TEXT("[FCM6SMOKE] FAIL %s"), Name);
	}
}

bool UFCM6SmokeSubsystem::Tick(const float DeltaTime)
{
	UWorld* World = GetWorld();
	const APlayerController* PC = World->GetFirstPlayerController();
	AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	UFCRunSubsystem* Run = World->GetSubsystem<UFCRunSubsystem>();
	if (Player == nullptr || Run == nullptr)
	{
		return true;
	}
	StepTime += DeltaTime;

	switch (Step)
	{
	case 0: // the slice pieces exist and the gate starts locked
		if (StepTime >= 2.0f)
		{
			AFCKeyItem* Key = FirstActor<AFCKeyItem>(World);
			AFCExtractZone* Extract = FirstActor<AFCExtractZone>(World);
			Check(TEXT("Slice: key + extraction spawned"), Key != nullptr && Extract != nullptr);
			Check(TEXT("Slice: extraction locked without the key"),
				Extract != nullptr && !Extract->CanInteract(Player));

			if (Key != nullptr)
			{
				Player->TeleportTo(Key->GetActorLocation() + FVector(120, 0, 40),
					FRotator(0, 180, 0), false, true);
				Key->Interact(Player, false);
			}
			Check(TEXT("Slice: key taken upstairs"), Run->HasKey());
			Step = 1;
			StepTime = 0.0f;
		}
		break;

	case 1: // commit at the pad and hold the window
		if (StepTime >= 1.0f)
		{
			AFCExtractZone* Extract = FirstActor<AFCExtractZone>(World);
			if (Extract == nullptr)
			{
				Check(TEXT("Slice: extraction present"), false);
				Finish();
				return false;
			}
			Player->TeleportTo(Extract->GetActorLocation() + FVector(0, 0, 100),
				FRotator(0, 90, 0), false, true);
			Check(TEXT("Slice: extraction unlocked by the key"), Extract->CanInteract(Player));
			if (const AFCWatcher* Watcher = FirstActor<AFCWatcher>(World))
			{
				WatcherDistAtCommit = FVector::Dist2D(
					Watcher->GetActorLocation(), Extract->GetActorLocation());
			}
			Extract->Interact(Player, false);
			Step = 2;
			StepTime = 0.0f;
		}
		break;

	case 2: // survive the commit window standing on the pad
		if (Run->IsWon() || StepTime >= 15.0f)
		{
			Check(TEXT("Slice: extraction commit window completed - ESCAPED"), Run->IsWon());
			if (const AFCWatcher* Watcher = FirstActor<AFCWatcher>(World))
			{
				const AFCExtractZone* Extract = FirstActor<AFCExtractZone>(World);
				const float DistNow = Extract != nullptr
					? FVector::Dist2D(Watcher->GetActorLocation(), Extract->GetActorLocation())
					: WatcherDistAtCommit;
				Check(TEXT("Slice: commit noise pulled the Watcher closer"),
					DistNow < WatcherDistAtCommit - 200.0f);
			}
			Finish();
			return false;
		}
		break;

	default:
		break;
	}
	return true;
}

void UFCM6SmokeSubsystem::Finish()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCM6SMOKE] DONE pass=%d fail=%d"), PassCount, FailCount);
	FPlatformMisc::RequestExit(false);
}

#else

void UFCM6SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UFCM6SmokeSubsystem::Deinitialize() { Super::Deinitialize(); }
bool UFCM6SmokeSubsystem::Tick(float) { return false; }
void UFCM6SmokeSubsystem::Check(const TCHAR*, bool) {}
void UFCM6SmokeSubsystem::Finish() {}

#endif
