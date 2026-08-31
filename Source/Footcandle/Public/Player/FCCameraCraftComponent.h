#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FCCameraCraftComponent.generated.h"

class UCameraComponent;
class USceneComponent;

// All first-person camera feel, as math (ADR-0003, ROADMAP 9.1): gait bob,
// idle breath, landing dip spring, lean roll/offset, sprint FOV, crouch eye
// lerp. No AnimSequences anywhere. Global motion scale honors accessibility
// (fc.Camera.MotionScale 0..1 - sliders-to-zero, ROADMAP 11.4).
UCLASS()
class FOOTCANDLE_API UFCCameraCraftComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFCCameraCraftComponent();

	void SetTargets(USceneComponent* InCameraRoot, UCameraComponent* InCamera);

	// -1 (left) .. +1 (right); 0 releases.
	void SetLeanAxis(float Axis) { LeanAxis = FMath::Clamp(Axis, -1.0f, 1.0f); }

	// NormalizedImpact 0..1 (landing softness to bone-rattle).
	void NotifyLanded(float NormalizedImpact);

	void SetGaitState(float GroundSpeed, bool bInSprinting, bool bInCrouched, bool bInListening);
	void SetEyeHeightOffset(float Offset) { TargetEyeOffset = Offset; }

	float GetCurrentLeanOffset() const { return CurrentLeanAmount; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	TObjectPtr<USceneComponent> CameraRoot;
	TObjectPtr<UCameraComponent> Camera;

	float BobPhase = 0.0f;
	float BreathPhase = 0.0f;
	float DipOffset = 0.0f;
	float DipVelocity = 0.0f;
	float LeanAxis = 0.0f;
	float CurrentLeanAmount = 0.0f;
	float GroundSpeed = 0.0f;
	float TargetEyeOffset = 0.0f;
	float CurrentEyeOffset = 0.0f;
	bool bSprinting = false;
	bool bCrouched = false;
	bool bListening = false;
};
