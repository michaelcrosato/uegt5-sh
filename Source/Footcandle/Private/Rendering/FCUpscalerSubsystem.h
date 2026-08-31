#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FCUpscalerSubsystem.generated.h"

// The one place upscaler policy is applied (ADR-0007). Settings UI (M10)
// and scalability tiers route through here; nothing else touches the cvars.
//
// Command line: -fcupscaler=off|tsr|dlaa|dlss-quality|dlss-balanced|dlss-perf
//               -fcrr  (adds DLSS Ray Reconstruction where supported)
// Console:      fc.Upscaler <mode> [rr]
UCLASS()
class UFCUpscalerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void ApplyMode(const FString& Mode, bool bRayReconstruction);
};
