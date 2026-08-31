#include "World/FCFlickerComponent.h"

#include "Components/LightComponent.h"
#include "Engine/World.h"
#include "FCGenSeed.h"
#include "FCGenStageIds.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<float> CVarFCFlickerScale(
	TEXT("fc.Flicker.Scale"),
	1.0f,
	TEXT("Global flicker amplitude 0..1. The photosensitivity accessibility ")
	TEXT("mode drives this toward 0 (ROADMAP 11.4)."));

UFCFlickerComponent::UFCFlickerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFCFlickerComponent::Configure(ULightComponent* InLight, const EFCFlickerStyle InStyle, const uint64 Seed)
{
	Light = InLight;
	Style = InStyle;
	Stream = FC::Gen::MakeStream(Seed, FC::Gen::StageId(FC::Gen::EStage::DevScene), 7ull);
	BaseIntensity = InLight != nullptr ? InLight->Intensity : 0.0f;
}

void UFCFlickerComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Light == nullptr)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float Amplitude = FMath::Clamp(CVarFCFlickerScale.GetValueOnGameThread(), 0.0f, 1.0f);

	// Resample the target at a capped rate; per-style character. Max resample
	// rates stay <= 8 Hz - under photosensitivity risk bands and slow enough
	// for temporal accumulation to follow.
	if (Now >= NextResampleTime)
	{
		switch (Style)
		{
		case EFCFlickerStyle::MainsHum:
			TargetScale = 1.0f - Amplitude * Stream.FRandRange(0.0f, 0.12f);
			NextResampleTime = Now + Stream.FRandRange(0.20f, 0.55f);
			break;
		case EFCFlickerStyle::FailingTube:
			// Mostly on; occasionally drops hard, then surges.
			TargetScale = Stream.FRand() < 0.18f
				? 1.0f - Amplitude * Stream.FRandRange(0.55f, 0.85f)
				: 1.0f - Amplitude * Stream.FRandRange(0.0f, 0.10f);
			NextResampleTime = Now + Stream.FRandRange(0.16f, 0.7f);
			break;
		case EFCFlickerStyle::Guttering:
			TargetScale = 0.55f + Amplitude * Stream.FRandRange(-0.35f, 0.25f);
			NextResampleTime = Now + Stream.FRandRange(0.25f, 0.9f);
			break;
		}
	}

	// Multi-frame interpolation toward the target - never a per-frame step.
	CurrentScale = FMath::FInterpTo(CurrentScale, TargetScale, DeltaTime, 9.0f);
	Light->SetIntensity(BaseIntensity * CurrentScale);
}
