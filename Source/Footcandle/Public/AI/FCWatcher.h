#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FCWatcher.generated.h"

class UCapsuleComponent;
class UFloatingPawnMovement;
class UNavigationInvokerComponent;
class UPointLightComponent;
class UStaticMeshComponent;
struct FFCNoiseEvent;

// Awareness states (ROADMAP 8.4): a hand-rolled C++ state machine
// (ADR-0005 - no Behavior Tree assets). Every transition logs its reason.
UENUM()
enum class EFCWatcherState : uint8
{
	Idle,
	Patrol,
	Suspicious,
	Investigate,
	Hunt,
	Search,
};

// THE WATCHER - the light hunter (ROADMAP 8.2). Tall, faceless, glides;
// never runs, never stops; zero animation clips: translation + sine sway +
// head yaw toward its interest point (ADR-0003). It hunts CONTRAST and
// LIGHT DELTAS: illuminated players, flashlight beams traced to their
// origin, lights that just changed. Its awareness is broadcast unambiguously
// through its eye-light (dim blue / amber / red) - the player must always
// know WHETHER it knows (P6).
UCLASS()
class FOOTCANDLE_API AFCWatcher : public APawn
{
	GENERATED_BODY()

public:
	AFCWatcher();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void SetPatrolPoints(const TArray<FVector>& Points) { PatrolPoints = Points; }

	EFCWatcherState GetState() const { return State; }
	float GetDetectionMeter() const { return DetectionMeter; }

#if !UE_BUILD_SHIPPING
	// Smoke-test hook: park it - Idle, zero meter, no patrol, no stale LKP -
	// so a scenario can stage it precisely. Never in shipping.
	void TestResetToIdle();
#endif

	// Tuning (CSV-bound later; explicit members now, no magic numbers inline).
	float SightRange = 3000.0f;
	float SightConeHalfAngleDeg = 55.0f;
	float HearingThreshold = 12.0f;
	float GlideSpeed = 230.0f;
	float ContactRange = 130.0f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UPointLightComponent> EyeLight;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UFloatingPawnMovement> Movement;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UNavigationInvokerComponent> NavInvoker;

private:
	void SetState(EFCWatcherState NewState, const TCHAR* Reason);
	void SenseTick();
	void OnNoise(const FFCNoiseEvent& Event);
	float ComputePlayerVisibility(const class AFCPlayerCharacter* Player) const;
	void MoveTowards(const FVector& Target, float DeltaSeconds);
	void UpdatePresentation(float DeltaSeconds);

	EFCWatcherState State = EFCWatcherState::Patrol;
	float DetectionMeter = 0.0f;
	FVector InterestPoint = FVector::ZeroVector;
	FVector LastKnownPlayerPos = FVector::ZeroVector;
	bool bHasLastKnownPlayerPos = false;
	float StateTimer = 0.0f;
	float SenseAccumulator = 0.0f;
	int32 PatrolIndex = 0;
	TArray<FVector> PatrolPoints;
	float BobPhase = 0.0f;
	FDelegateHandle NoiseHandle;
};
