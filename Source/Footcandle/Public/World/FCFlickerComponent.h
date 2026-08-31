#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FCFlickerComponent.generated.h"

class ULightComponent;

UENUM()
enum class EFCFlickerStyle : uint8
{
	MainsHum,    // gentle intensity breathing - healthy fixture character
	FailingTube, // sputtering fluorescent - drops out, surges back
	Guttering,   // low, unstable - a light about to die
};

// The ONE flicker mechanism (ROADMAP 6.2). Every flickering light in the
// game routes through this component, because flicker has two hard masters:
//  1. Temporal denoisers: per-frame random intensity breaks Lumen/MegaLights
//     accumulation (ghost/boil). Changes here are resampled at a capped rate
//     and interpolated over multiple frames, never stepped per frame.
//  2. Photosensitivity (ROADMAP 11.4): fc.Flicker.Scale globally limits
//     amplitude, and the resample rate is clamped below seizure-risk bands.
// Deterministic per instance: seeded from FC::Gen so captures regress cleanly.
UCLASS()
class FOOTCANDLE_API UFCFlickerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFCFlickerComponent();

	void Configure(ULightComponent* InLight, EFCFlickerStyle InStyle, uint64 Seed);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	TObjectPtr<ULightComponent> Light;
	EFCFlickerStyle Style = EFCFlickerStyle::MainsHum;
	FRandomStream Stream;
	float BaseIntensity = 0.0f;
	float CurrentScale = 1.0f;
	float TargetScale = 1.0f;
	float NextResampleTime = 0.0f;
};
