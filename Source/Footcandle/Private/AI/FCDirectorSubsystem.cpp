#include "AI/FCDirectorSubsystem.h"

#include "AI/FCWatcher.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "Misc/CommandLine.h"
#include "Noise/FCNoiseSubsystem.h"

void UFCDirectorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.IsGameWorld() && FParse::Param(FCommandLine::Get(), TEXT("fcdirector")))
	{
		EnableNow();
	}
}

void UFCDirectorSubsystem::EnableNow()
{
	if (bEnabled)
	{
		return;
	}
	bEnabled = true;

	// Player noise feeds pressure: a loud run is a hunted run.
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		NoiseHandle = Noise->OnNoiseEmitted.AddLambda([this](const FFCNoiseEvent& Event)
		{
			if (Event.Loudness >= 40.0f)
			{
				AddPressure(Event.Loudness * 0.02f, TEXT("loud noise"));
			}
		});
	}
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCDirectorSubsystem::Tick), 1.0f);
	UE_LOG(LogFootcandle, Display, TEXT("[FCDIRECTOR] enabled"));
}

void UFCDirectorSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	Super::Deinitialize();
}

void UFCDirectorSubsystem::AddPressure(const float Amount, const TCHAR* Reason)
{
	if (!bEnabled)
	{
		return;
	}
	// The rest guarantee: pressure cannot spike during the post-contact lull.
	if (GetWorld()->GetTimeSeconds() < RestUntil)
	{
		return;
	}
	Pressure = FMath::Clamp(Pressure + Amount, 0.0f, 100.0f);
	UE_LOG(LogFootcandle, Display, TEXT("[FCDIRECTOR] pressure %.1f (+%.1f: %s)"),
		Pressure, Amount, Reason);
}

void UFCDirectorSubsystem::NotifyContactResolved()
{
	RestUntil = GetWorld()->GetTimeSeconds() + RestSeconds;
	Pressure = FMath::Max(Pressure - 25.0f, 0.0f);
	UE_LOG(LogFootcandle, Display, TEXT("[FCDIRECTOR] contact resolved - rest until t=%.0f"), RestUntil);
}

bool UFCDirectorSubsystem::Tick(float /*DeltaTime*/)
{
	if (!bEnabled)
	{
		return false;
	}
	// Time pressure: +1 per 90 s.
	TimeAccumulator += 1.0f;
	if (TimeAccumulator >= 90.0f)
	{
		TimeAccumulator = 0.0f;
		AddPressure(1.0f, TEXT("time"));
	}

	// Spawn the hunter when the run earns it.
	if (!Hunter.IsValid() && Pressure >= SpawnHunterAtPressure
		&& GetWorld()->GetTimeSeconds() >= RestUntil)
	{
		const APlayerController* PC = GetWorld()->GetFirstPlayerController();
		const APawn* Pawn = PC != nullptr ? PC->GetPawn() : nullptr;
		if (Pawn != nullptr)
		{
			// Spawn out of sight up the street (spawn proximity scales with
			// pressure later; fixed offset for the core).
			const FVector SpawnPos = Pawn->GetActorLocation() + FVector(3200, -600, 30);
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			if (AFCWatcher* Spawned = GetWorld()->SpawnActor<AFCWatcher>(SpawnPos, FRotator(0, 180, 0), Params))
			{
				Spawned->SetPatrolPoints({ SpawnPos, Pawn->GetActorLocation() + FVector(-2500, -600, 30) });
				Hunter = Spawned;
				UE_LOG(LogFootcandle, Display, TEXT("[FCDIRECTOR] hunter deployed at pressure %.0f"), Pressure);
			}
		}
	}

	// Aggression scales with pressure.
	if (AFCWatcher* ActiveHunter = Hunter.Get())
	{
		ActiveHunter->HearingThreshold = FMath::Lerp(14.0f, 8.0f, Pressure / 100.0f);
		ActiveHunter->GlideSpeed = FMath::Lerp(210.0f, 255.0f, Pressure / 100.0f);
	}
	return true;
}
