#include "Testing/FCM8SmokeSubsystem.h"

#include "AI/FCDirectorSubsystem.h"
#include "AI/FCWatcher.h"
#include "Components/LightComponent.h"
#include "Engine/SpotLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Footcandle.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Objectives/FCExtractZone.h"
#include "Objectives/FCKeyItem.h"
#include "Objectives/FCRunSubsystem.h"
#include "Player/FCPlayerCharacter.h"
#include "World/FCBreakerPanel.h"

#if !UE_BUILD_SHIPPING

namespace
{
	template <typename T>
	T* FirstActorM8(UWorld* World)
	{
		for (TActorIterator<T> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

void UFCM8SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("fcm8smoke")))
	{
		return;
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCM8SMOKE] starting"));
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCM8SmokeSubsystem::Tick));
}

void UFCM8SmokeSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	Super::Deinitialize();
}

void UFCM8SmokeSubsystem::Check(const TCHAR* Name, const bool bCondition)
{
	if (bCondition)
	{
		++PassCount;
		UE_LOG(LogFootcandle, Display, TEXT("[FCM8SMOKE] PASS %s"), Name);
	}
	else
	{
		++FailCount;
		UE_LOG(LogFootcandle, Error, TEXT("[FCM8SMOKE] FAIL %s"), Name);
	}
}

bool UFCM8SmokeSubsystem::Tick(const float DeltaTime)
{
	UWorld* World = GetWorld();
	const APlayerController* PC = World->GetFirstPlayerController();
	AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	UFCRunSubsystem* Run = World->GetSubsystem<UFCRunSubsystem>();
	UFCDirectorSubsystem* Director = World->GetSubsystem<UFCDirectorSubsystem>();
	if (Player == nullptr || Run == nullptr || Director == nullptr)
	{
		return true;
	}
	StepTime += DeltaTime;

	switch (Step)
	{
	case 0: // dark streets, locked gate, quiet director
		if (StepTime >= 2.5f)
		{
			const ASpotLight* AnyStreet = FirstActorM8<ASpotLight>(World);
			AFCExtractZone* Extract = FirstActorM8<AFCExtractZone>(World);
			Check(TEXT("Run: 2 conditions required, 0 satisfied"),
				Run->GetConditionsRequired() == 2 && Run->GetConditionsSatisfied() == 0);
			Check(TEXT("Run: streets start DARK (grid decision)"),
				AnyStreet != nullptr && !AnyStreet->GetLightComponent()->IsVisible());
			Check(TEXT("Run: extraction locked"),
				Extract != nullptr && !Extract->CanInteract(Player));
			Check(TEXT("Director: no hunter yet"), !Director->IsHunterActive());

			// Condition 1: the key.
			if (AFCKeyItem* Key = FirstActorM8<AFCKeyItem>(World))
			{
				Player->TeleportTo(Key->GetActorLocation() + FVector(120, 0, 40), FRotator(0, 180, 0), false, true);
				Key->Interact(Player, false);
			}
			Check(TEXT("Run: condition 1/2 after the key"), Run->GetConditionsSatisfied() == 1);
			Check(TEXT("Run: still locked at 1/2"),
				Extract != nullptr && !Extract->CanInteract(Player));

			// Condition 2: throw the street substation.
			if (AFCBreakerPanel* Substation = FirstActorM8<AFCBreakerPanel>(World))
			{
				Player->TeleportTo(Substation->GetActorLocation() + FVector(120, 0, 20), FRotator(0, 180, 0), false, true);
				Substation->Interact(Player, false);
			}
			Step = 1;
			StepTime = 0.0f;
		}
		break;

	case 1: // powered streets, full conditions, director escalating
		if (StepTime >= 4.0f)
		{
			const ASpotLight* AnyStreet = FirstActorM8<ASpotLight>(World);
			Check(TEXT("Run: streets LIT after the substation"),
				AnyStreet != nullptr && AnyStreet->GetLightComponent()->IsVisible());
			Check(TEXT("Run: 2/2 conditions"), Run->GetConditionsSatisfied() == 2);
			Check(TEXT("Director: pressure risen (>=30 from conditions)"),
				Director->GetPressure() >= 29.0f);
			Check(TEXT("Director: hunter deployed past threshold"), Director->IsHunterActive());

			// Rest guarantee: a resolved contact must block pressure spikes.
			const float Before = Director->GetPressure();
			Director->NotifyContactResolved();
			Director->AddPressure(50.0f, TEXT("smoke probe"));
			Check(TEXT("Director: rest guarantee blocks pressure"),
				Director->GetPressure() <= Before - 20.0f);

			// Extract.
			if (AFCExtractZone* Extract = FirstActorM8<AFCExtractZone>(World))
			{
				Player->TeleportTo(Extract->GetActorLocation() + FVector(0, 0, 100), FRotator(0, 90, 0), false, true);
				Check(TEXT("Run: extraction unlocked at 2/2"), Extract->CanInteract(Player));
				Extract->Interact(Player, false);
			}
			Step = 2;
			StepTime = 0.0f;
		}
		break;

	case 2: // survive the window
		if (Run->IsWon() || StepTime >= 15.0f)
		{
			Check(TEXT("Run: ESCAPED through the systemic gate"), Run->IsWon());
			Finish();
			return false;
		}
		break;

	default:
		break;
	}
	return true;
}

void UFCM8SmokeSubsystem::Finish()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCM8SMOKE] DONE pass=%d fail=%d"), PassCount, FailCount);
	FPlatformMisc::RequestExit(false);
}

#else

void UFCM8SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UFCM8SmokeSubsystem::Deinitialize() { Super::Deinitialize(); }
bool UFCM8SmokeSubsystem::Tick(float) { return false; }
void UFCM8SmokeSubsystem::Check(const TCHAR*, bool) {}
void UFCM8SmokeSubsystem::Finish() {}

#endif
