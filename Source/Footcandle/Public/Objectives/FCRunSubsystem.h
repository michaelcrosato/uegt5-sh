#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCRunSubsystem.generated.h"

// Run state (M8: systemic multi-condition gate; ROADMAP 4.3). Conditions are
// placed by the run spawner (key retrieval, power restoration, ...); each
// satisfied condition raises Director Pressure - the run gets harder as it
// gets closer to won.
UCLASS()
class FOOTCANDLE_API UFCRunSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void SetConditionsRequired(const int32 Count) { ConditionsRequired = Count; }
	void NotifyConditionSatisfied(const TCHAR* Reason);
	void NotifyExtractionStarted();
	void NotifyExtractionComplete();

	// All conditions met = the extraction gate is live.
	bool HasKey() const { return ConditionsSatisfied >= ConditionsRequired; }
	int32 GetConditionsSatisfied() const { return ConditionsSatisfied; }
	int32 GetConditionsRequired() const { return ConditionsRequired; }
	bool IsWon() const { return bWon; }

	// Back-compat wrapper for the M6 slice pieces.
	void NotifyKeyTaken() { NotifyConditionSatisfied(TEXT("key taken")); }

	int32 NoiseEventCount = 0;

private:
	int32 ConditionsRequired = 1;
	int32 ConditionsSatisfied = 0;
	bool bWon = false;
};
