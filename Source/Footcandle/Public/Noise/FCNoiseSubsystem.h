#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCNoiseSubsystem.generated.h"

// One noise event, one model, two consumers (pillar P5, ROADMAP 7).
USTRUCT(BlueprintType)
struct FFCNoiseEvent
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	// 0-100 perceptual scale (ROADMAP 7.3).
	UPROPERTY()
	float Loudness = 0.0f;

	// Noise.Source.* vocabulary - plain FName until the GameplayTags pass.
	UPROPERTY()
	FName SourceTag;

	UPROPERTY()
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY()
	float Timestamp = 0.0f;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FFCOnNoiseEmitted, const FFCNoiseEvent&);

// The single funnel for every gameplay sound event. M1 ships the API and
// debug view; M3 replaces the internals with portal-graph propagation
// (Dijkstra over the room graph) without changing a single call site.
// Consumers: enemy hearing (M4) and the audio mix (M3) - never divergent.
UCLASS()
class FOOTCANDLE_API UFCNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void EmitNoise(const FVector& Origin, float Loudness, FName SourceTag, AActor* Instigator);

	FFCOnNoiseEmitted OnNoiseEmitted;

	const TArray<FFCNoiseEvent>& GetRecentEvents() const { return RecentEvents; }

private:
	TArray<FFCNoiseEvent> RecentEvents; // ring of the last N for debug/AI
};
