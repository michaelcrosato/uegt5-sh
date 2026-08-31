#include "Testing/FCM9SmokeSubsystem.h"

#include "AI/FCListener.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Footcandle.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Player/FCPlayerCharacter.h"

#if !UE_BUILD_SHIPPING

namespace
{
	AFCListener* FindListener(UWorld* World)
	{
		for (TActorIterator<AFCListener> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}
}

void UFCM9SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("fcm9smoke")))
	{
		return;
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCM9SMOKE] starting"));
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCM9SmokeSubsystem::Tick));
}

void UFCM9SmokeSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	Super::Deinitialize();
}

void UFCM9SmokeSubsystem::Check(const TCHAR* Name, const bool bCondition)
{
	if (bCondition)
	{
		++PassCount;
		UE_LOG(LogFootcandle, Display, TEXT("[FCM9SMOKE] PASS %s"), Name);
	}
	else
	{
		++FailCount;
		UE_LOG(LogFootcandle, Error, TEXT("[FCM9SMOKE] FAIL %s"), Name);
	}
}

bool UFCM9SmokeSubsystem::Tick(const float DeltaTime)
{
	UWorld* World = GetWorld();
	AFCListener* Listener = FindListener(World);
	UFCNoiseSubsystem* Noise = World->GetSubsystem<UFCNoiseSubsystem>();
	if (Listener == nullptr || Noise == nullptr)
	{
		return true;
	}
	StepTime += DeltaTime;

	switch (Step)
	{
	case 0: // silence: it must be frozen (the spawn hitch noise dies fast)
		if (StepTime >= 6.0f)
		{
			Check(TEXT("Listener: frozen in silence"), !Listener->CanMoveNow());
			FrozenPos = Listener->GetActorLocation();
			// A crash up the street: window opens, it investigates precisely.
			Noise->EmitNoise(Listener->GetActorLocation() + FVector(-1400, 60, 0),
				65.0f, TEXT("Noise.Source.Impact"), nullptr);
			Step = 1;
			StepTime = 0.0f;
		}
		break;

	case 1: // moves on noise, arrives near it
		if (StepTime >= 9.0f)
		{
			const FVector NoisePos = FrozenPos + FVector(-1400, 60, 0);
			Check(TEXT("Listener: noise opened its movement window and it closed in"),
				FVector::Dist2D(Listener->GetActorLocation(), NoisePos) < 400.0f);

			// Back to silence: it should freeze again.
			FrozenPos = Listener->GetActorLocation();
			Step = 2;
			StepTime = 0.0f;
		}
		break;

	case 2: // re-frozen after the window expires
		if (StepTime >= 6.0f)
		{
			Check(TEXT("Listener: re-frozen when the world went quiet"),
				!Listener->CanMoveNow()
				&& FVector::Dist2D(Listener->GetActorLocation(), FrozenPos) < 120.0f);

			// Rain: masks quiet sounds AND unlocks its mobility.
			FFCNoiseEvent Sneak;
			Sneak.Origin = Listener->GetActorLocation() + FVector(800, 0, 0);
			Sneak.Loudness = 8.0f; // a sneak step
			const float HeardClear = Noise->PerceivedLoudnessAt(Sneak, Listener->GetActorLocation());
			Noise->SetAmbientNoiseFloor(12.0f);
			const float HeardRain = Noise->PerceivedLoudnessAt(Sneak, Listener->GetActorLocation());
			Check(TEXT("Rain: sneak steps audible on a clear night"), HeardClear > 0.0f);
			Check(TEXT("Rain: the floor masks sneak steps entirely"), HeardRain == 0.0f);

			RainStartPos = Listener->GetActorLocation();
			Step = 3;
			StepTime = 0.0f;
		}
		break;

	case 3: // under rain it patrols without any noise event
		if (StepTime >= 5.0f)
		{
			Check(TEXT("Rain: the floor keeps the Listener mobile (it patrols)"),
				Listener->CanMoveNow()
				&& FVector::Dist2D(Listener->GetActorLocation(), RainStartPos) > 250.0f);
			Finish();
			return false;
		}
		break;

	default:
		break;
	}
	return true;
}

void UFCM9SmokeSubsystem::Finish()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCM9SMOKE] DONE pass=%d fail=%d"), PassCount, FailCount);
	FPlatformMisc::RequestExit(false);
}

#else

void UFCM9SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UFCM9SmokeSubsystem::Deinitialize() { Super::Deinitialize(); }
bool UFCM9SmokeSubsystem::Tick(float) { return false; }
void UFCM9SmokeSubsystem::Check(const TCHAR*, bool) {}
void UFCM9SmokeSubsystem::Finish() {}

#endif
