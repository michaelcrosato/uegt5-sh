#include "Player/FCCameraCraftComponent.h"

#include "Camera/CameraComponent.h"
#include "FCTuningSettings.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<float> CVarFCCameraMotionScale(
	TEXT("fc.Camera.MotionScale"),
	1.0f,
	TEXT("Global scale on procedural camera motion (bob/breath/dip). ")
	TEXT("0 disables everything except lean - the accessibility floor."));

UFCCameraCraftComponent::UFCCameraCraftComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UFCCameraCraftComponent::SetTargets(USceneComponent* InCameraRoot, UCameraComponent* InCamera)
{
	CameraRoot = InCameraRoot;
	Camera = InCamera;
}

void UFCCameraCraftComponent::NotifyLanded(const float NormalizedImpact)
{
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	DipVelocity -= Tuning->LandingDipAmplitude * FMath::Clamp(NormalizedImpact, 0.0f, 1.0f) * 10.0f;
}

void UFCCameraCraftComponent::SetGaitState(const float InGroundSpeed, const bool bInSprinting,
	const bool bInCrouched, const bool bInListening)
{
	GroundSpeed = InGroundSpeed;
	bSprinting = bInSprinting;
	bCrouched = bInCrouched;
	bListening = bInListening;
}

void UFCCameraCraftComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CameraRoot == nullptr || Camera == nullptr)
	{
		return;
	}

	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	const float MotionScale = FMath::Clamp(CVarFCCameraMotionScale.GetValueOnGameThread(), 0.0f, 1.0f);

	// Gait bob: amplitude scales with speed, frequency with gait. Suppressed
	// while listening (the held-breath verb steadies everything).
	const float SpeedAlpha = FMath::Clamp(GroundSpeed / Tuning->SprintSpeed, 0.0f, 1.0f);
	const float BobAmp = bListening ? 0.0f : Tuning->BobAmplitude * SpeedAlpha * MotionScale;
	const float BobFreq = Tuning->BobFrequency * (0.6f + 1.1f * SpeedAlpha) * (bSprinting ? 1.25f : 1.0f);
	if (GroundSpeed > 20.0f)
	{
		BobPhase += DeltaTime * BobFreq * 2.0f * PI;
	}
	const float Bob = FMath::Sin(BobPhase) * BobAmp;

	// Idle breath - tiny, always present unless listening or disabled.
	BreathPhase += DeltaTime * 2.0f * PI * 0.24f;
	const float Breath = bListening ? 0.0f
		: FMath::Sin(BreathPhase) * Tuning->BreathAmplitude * MotionScale;

	// Landing dip: damped spring toward zero.
	const float SpringAccel = -Tuning->LandingDipStiffness * DipOffset - Tuning->LandingDipDamping * DipVelocity;
	DipVelocity += SpringAccel * DeltaTime;
	DipOffset += DipVelocity * DeltaTime;

	// Lean: exponential approach; roll + lateral offset.
	CurrentLeanAmount = FMath::FInterpTo(CurrentLeanAmount, LeanAxis, DeltaTime, Tuning->LeanSpeed);

	// Crouch eye height lerp.
	CurrentEyeOffset = FMath::FInterpTo(CurrentEyeOffset, TargetEyeOffset, DeltaTime, 10.0f);

	const float DipScaled = DipOffset * MotionScale;
	CameraRoot->SetRelativeLocation(FVector(
		0.0f,
		CurrentLeanAmount * Tuning->LeanOffset,
		CurrentEyeOffset + Bob + Breath + DipScaled));
	CameraRoot->SetRelativeRotation(FRotator(0.0f, 0.0f, CurrentLeanAmount * Tuning->LeanRoll));

	// Sprint FOV nudge.
	const float TargetFOV = Tuning->BaseFOV + (bSprinting ? Tuning->SprintFOVAdd * SpeedAlpha : 0.0f);
	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFOV, DeltaTime, 6.0f));
}
