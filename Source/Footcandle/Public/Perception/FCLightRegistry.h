#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCLightRegistry.generated.h"

class ULightComponent;
class ULocalLightComponent;
class USpotLightComponent;

// The gameplay light model (ROADMAP 8.3): a CPU-side registry of every
// gameplay-relevant light. Enemy sight samples THIS, never the renderer -
// Lumen output is GPU-side, temporal, and non-deterministic, which makes it
// untestable and unfair. Tuned so "looks dark" and "is dark" agree (AI-07).
//
// On top of the scalar lux: light-state DELTAS (a light that just changed
// draws attention - the Watcher's food) and BEAM registration (a flashlight
// cone is a detectable object pointing home).
UCLASS()
class FOOTCANDLE_API UFCLightRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterLight(ULightComponent* Light);

	// Illumination 0..1 at a point: sum of intensity x attenuation x cone x
	// occlusion (one visibility trace per light, near lights only).
	float SampleIllumination(const FVector& Point, const AActor* IgnoreActor = nullptr) const;

	// A light whose visible state changed calls this (switches, breakers,
	// flicker deaths). Recent changes carry an attention bonus.
	void NotifyLightStateChanged(const FVector& Location);

	// Seconds since a light state changed within Radius of Point (BIG_NUMBER
	// if never) - the Watcher's delta sense.
	float TimeSinceNearbyLightChange(const FVector& Point, float Radius) const;

	// Active flashlight-style beams: origin + direction + length. The Watcher
	// traces visible beams back toward their origin.
	void SetActiveBeam(AActor* Owner, const FVector& Origin, const FVector& Direction, float Length, bool bActive);

	struct FBeam
	{
		TWeakObjectPtr<AActor> Owner;
		FVector Origin = FVector::ZeroVector;
		FVector Direction = FVector::ForwardVector;
		float Length = 0.0f;
	};
	const TArray<FBeam>& GetActiveBeams() const { return Beams; }

private:
	TArray<TWeakObjectPtr<ULightComponent>> Lights;
	TArray<FBeam> Beams;

	struct FLightChange
	{
		FVector Location = FVector::ZeroVector;
		float Time = -1000.0f;
	};
	TArray<FLightChange> RecentChanges;
};
