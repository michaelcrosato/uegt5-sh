#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCDirectorSubsystem.generated.h"

class AFCWatcher;

// The Director (ROADMAP 8.5): pacing as a system. Pressure rises with
// satisfied conditions, elapsed time, and the player's noise history; it
// spawns and sharpens the hunter. The REST GUARANTEE is non-negotiable:
// after a resolved contact, no new pressure spike for RestSeconds - without
// it, procedural pacing degrades into harassment (P6).
UCLASS()
class FOOTCANDLE_API UFCDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	float GetPressure() const { return Pressure; }
	bool IsHunterActive() const { return Hunter.IsValid(); }

	// The default boot path enables the Director without command-line flags.
	void EnableNow();

	void AddPressure(float Amount, const TCHAR* Reason);
	void NotifyContactResolved();

	// Tuning (CSV-bound later).
	float SpawnHunterAtPressure = 20.0f;
	float RestSeconds = 90.0f;

private:
	bool Tick(float DeltaTime);

	FTSTicker::FDelegateHandle TickerHandle;
	float Pressure = 0.0f;
	float RestUntil = -1.0f;
	float TimeAccumulator = 0.0f;
	bool bEnabled = false;
	TWeakObjectPtr<AFCWatcher> Hunter;
	FDelegateHandle NoiseHandle;
};
