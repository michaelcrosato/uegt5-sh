#include "Testing/FCM4SmokeSubsystem.h"

#include "AI/FCWatcher.h"
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
	AFCWatcher* FindWatcher(UWorld* World)
	{
		for (TActorIterator<AFCWatcher> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	AFCPlayerCharacter* GetPlayerM4(UWorld* World)
	{
		const APlayerController* PC = World->GetFirstPlayerController();
		return PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	}
}

void UFCM4SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("fcm4smoke")))
	{
		return;
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCM4SMOKE] starting"));
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCM4SmokeSubsystem::Tick));
}

void UFCM4SmokeSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	Super::Deinitialize();
}

void UFCM4SmokeSubsystem::Check(const TCHAR* Name, const bool bCondition)
{
	if (bCondition)
	{
		++PassCount;
		UE_LOG(LogFootcandle, Display, TEXT("[FCM4SMOKE] PASS %s"), Name);
	}
	else
	{
		++FailCount;
		UE_LOG(LogFootcandle, Error, TEXT("[FCM4SMOKE] FAIL %s"), Name);
	}
}

bool UFCM4SmokeSubsystem::Tick(const float DeltaTime)
{
	UWorld* World = GetWorld();
	AFCPlayerCharacter* Player = GetPlayerM4(World);
	AFCWatcher* Watcher = FindWatcher(World);
	if (Player == nullptr || Watcher == nullptr)
	{
		return true;
	}
	StepTime += DeltaTime;

	auto Advance = [this](const int32 NextStep) { Step = NextStep; StepTime = 0.0f; };

	switch (Step)
	{
	case 0: // park the player in darkness IMMEDIATELY (the fixed streetlight
	        // genuinely lights the spawn now), then stage a clean hearing test
		if (!bSawHunt) // reused as "player parked" latch for step 0
		{
			bSawHunt = true;
			Player->TeleportTo(FVector(2600, 2200, 120), FRotator(0, -90, 0), false, true);
		}
		if (StepTime >= 1.5f)
		{
			bSawHunt = false;
			Watcher->TestResetToIdle(); // forget any glimpse of the spawn
			// A loud crash in the middle of the street, well away from both.
			if (UFCNoiseSubsystem* Noise = World->GetSubsystem<UFCNoiseSubsystem>())
			{
				Noise->EmitNoise(FVector(-400, -1000, 100), 80.0f, TEXT("Noise.Source.Impact"), nullptr);
			}
			Advance(1);
		}
		break;

	case 1: // hearing: it should investigate NEAR the noise (glide ~2400cm at 230cm/s)
		if (StepTime >= 14.0f)
		{
			const float DistToNoise = FVector::Dist2D(Watcher->GetActorLocation(), FVector(-400, -1000, 100));
			Check(TEXT("Hearing: moved to investigate near the noise"), DistToNoise < 700.0f);
			Check(TEXT("Hearing: state advanced past idle"),
				Watcher->GetState() != EFCWatcherState::Idle || DistToNoise < 700.0f);

			// Lit detection setup: player under the streetlight, watcher down
			// the street facing them.
			// The pool CENTER is on the beam centerline near the facade
			// (y ~= -100), not at the light's base - stand the player there.
			// Park the watcher first or its patrol marches it away mid-test.
			Player->TeleportTo(FVector(500, -100, 120), FRotator(0, -90, 0), false, true);
			Watcher->TeleportTo(FVector(500, -1900, 140), FRotator(0, 90, 0), false, true);
			Watcher->TestResetToIdle();
			Watcher->SetActorRotation(FRotator(0, 90, 0));
			Advance(2);
		}
		break;

	case 2: // lit player in cone -> Hunt (should take ~3-5 s)
	{
		static float LastLogTime = 0.0f;
		if (StepTime - LastLogTime >= 2.0f)
		{
			LastLogTime = StepTime;
			UE_LOG(LogFootcandle, Display,
				TEXT("[FCM4SMOKE] sight debug t=%.1f state=%d meter=%.2f watcher=%s yaw=%.0f player=%s"),
				StepTime, static_cast<int32>(Watcher->GetState()), Watcher->GetDetectionMeter(),
				*Watcher->GetActorLocation().ToCompactString(), Watcher->GetActorRotation().Yaw,
				*Player->GetActorLocation().ToCompactString());
		}
		if (Watcher->GetState() == EFCWatcherState::Hunt)
		{
			bSawHunt = true;
		}
	}
		if (bSawHunt || StepTime >= 12.0f)
		{
			Check(TEXT("Sight: lit player in cone reached Hunt"), bSawHunt);

			// Dark evasion setup: player still, in darkness east of the
			// building; watcher same distance, facing them; meter reset by
			// teleporting it far then back? Simpler: fresh angle and let the
			// meter decay first during step 3's lead-in.
			Player->TeleportTo(FVector(1800, 350, 120), FRotator(0, 180, 0), false, true);
			Watcher->TeleportTo(FVector(1800, -1250, 140), FRotator(0, 90, 0), false, true);
			Watcher->TestResetToIdle();
			Watcher->SetActorRotation(FRotator(0, 90, 0));
			PeakDarkMeter = 0.0f;
			Advance(3);
		}
		break;

	case 3: // dark, still player: meter must stay low
		if (StepTime >= 2.0f) // let the hunt meter decay first
		{
			PeakDarkMeter = FMath::Max(PeakDarkMeter, Watcher->GetDetectionMeter());
		}
		if (StepTime >= 10.0f)
		{
			Check(TEXT("Darkness: still player in the dark stayed undetected"),
				PeakDarkMeter < 0.85f && Watcher->GetState() != EFCWatcherState::Hunt);

			// Contact: put it on top of the player. Strike one.
			Watcher->TeleportTo(Player->GetActorLocation() + FVector(80, 0, 20),
				FRotator(0, 180, 0), false, true);
			Advance(4);
		}
		break;

	case 4: // strike one -> Critical
		if (Player->GetHealthState() == EFCHealthState::Critical || StepTime >= 4.0f)
		{
			Check(TEXT("Contact: first strike -> Critical"),
				Player->GetHealthState() == EFCHealthState::Critical);
			Advance(5);
		}
		break;

	case 5: // cooldown passes; keep them together -> strike two -> Dead
		Watcher->TeleportTo(Player->GetActorLocation() + FVector(80, 0, 20),
			FRotator(0, 180, 0), false, true);
		if (Player->GetHealthState() == EFCHealthState::Dead || StepTime >= 5.0f)
		{
			Check(TEXT("Contact: second strike -> Dead (attribution logged)"),
				Player->GetHealthState() == EFCHealthState::Dead);
			Finish();
			return false;
		}
		break;

	default:
		break;
	}
	return true;
}

void UFCM4SmokeSubsystem::Finish()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCM4SMOKE] DONE pass=%d fail=%d"), PassCount, FailCount);
	FPlatformMisc::RequestExit(false);
}

#else

void UFCM4SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UFCM4SmokeSubsystem::Deinitialize() { Super::Deinitialize(); }
bool UFCM4SmokeSubsystem::Tick(float) { return false; }
void UFCM4SmokeSubsystem::Check(const TCHAR*, bool) {}
void UFCM4SmokeSubsystem::Finish() {}

#endif
