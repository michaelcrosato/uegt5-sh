#pragma once

#include "CoreMinimal.h"
#include "FCRoomGraph.h"
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

// The single funnel for every gameplay sound event, and - since M3 - the
// portal-graph propagation model behind it (ROADMAP 7.2). One model, two
// consumers: enemy hearing (M4) and the audio mix. Never divergent (P5).
UCLASS()
class FOOTCANDLE_API UFCNoiseSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void EmitNoise(const FVector& Origin, float Loudness, FName SourceTag, AActor* Instigator);

	// --- Acoustic topology (registered by scene builders / the generator) ---
	FC::Gen::FRoomGraph& GetRoomGraph() { return RoomGraph; }
	void SetPortalState(int32 PortalId, FC::Gen::EAperture State);

	// Perceived loudness of an event at a listener position, through the
	// graph. This is THE hearing query - the M4 enemy consumes exactly this.
	float PerceivedLoudnessAt(const FFCNoiseEvent& Event, const FVector& ListenerLocation) const;

	FFCOnNoiseEmitted OnNoiseEmitted;

	const TArray<FFCNoiseEvent>& GetRecentEvents() const { return RecentEvents; }

private:
	FC::Gen::FRoomGraph RoomGraph;
	TArray<FFCNoiseEvent> RecentEvents; // ring of the last N for debug/AI
};
