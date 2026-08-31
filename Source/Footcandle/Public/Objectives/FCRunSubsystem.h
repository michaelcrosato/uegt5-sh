#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCRunSubsystem.generated.h"

// Run state (M6 minimal): the escape loop's bookkeeping. M8 replaces the
// single hand-wired key with systemic conditions + Pressure + the Director.
UCLASS()
class FOOTCANDLE_API UFCRunSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void NotifyKeyTaken();
	void NotifyExtractionStarted();
	void NotifyExtractionComplete();

	bool HasKey() const { return bHasKey; }
	bool IsWon() const { return bWon; }

	// Run stats (shown on the victory screen at M10; logged now).
	int32 NoiseEventCount = 0;

private:
	bool bHasKey = false;
	bool bWon = false;
};
