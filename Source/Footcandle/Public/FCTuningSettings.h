#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "FCTuningSettings.generated.h"

// All gameplay tunables (AGENTS.md rule 5: no magic numbers in code).
// Backed by DefaultGame.ini - text, diffable, hot-editable. Values below are
// the roadmap's starting numbers; playtest tuning edits the ini, not code.
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "FOOTCANDLE Tuning"))
class FOOTCANDLE_API UFCTuningSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UFCTuningSettings* Get() { return GetDefault<UFCTuningSettings>(); }

	// --- Movement (cm/s) ---
	UPROPERTY(config, EditAnywhere, Category = "Movement")
	float WalkSpeed = 260.0f;

	UPROPERTY(config, EditAnywhere, Category = "Movement")
	float SneakSpeed = 143.0f;

	UPROPERTY(config, EditAnywhere, Category = "Movement")
	float SprintSpeed = 500.0f;

	UPROPERTY(config, EditAnywhere, Category = "Movement")
	float CrouchSpeed = 150.0f;

	UPROPERTY(config, EditAnywhere, Category = "Movement", meta = (ToolTip = "Eye height while standing, cm (art-direction: pick once, never change)"))
	float EyeHeight = 165.0f;

	UPROPERTY(config, EditAnywhere, Category = "Movement")
	float CrouchedEyeHeight = 105.0f;

	// --- Stamina (percent, per second) ---
	UPROPERTY(config, EditAnywhere, Category = "Stamina")
	float StaminaMax = 100.0f;

	UPROPERTY(config, EditAnywhere, Category = "Stamina")
	float SprintDrainPerSecond = 12.0f;

	UPROPERTY(config, EditAnywhere, Category = "Stamina")
	float VaultCost = 8.0f;

	UPROPERTY(config, EditAnywhere, Category = "Stamina")
	float RegenPerSecondMoving = 6.0f;

	UPROPERTY(config, EditAnywhere, Category = "Stamina")
	float RegenPerSecondStill = 15.0f;

	UPROPERTY(config, EditAnywhere, Category = "Stamina")
	float RegenSuppressSeconds = 1.5f;

	UPROPERTY(config, EditAnywhere, Category = "Stamina", meta = (ToolTip = "Below this, breathing gets loud (+ExhaustedNoise for 8 s)"))
	float ExhaustedThreshold = 15.0f;

	// --- Camera craft (all slider-to-zero-able for comfort, ROADMAP 11.4) ---
	UPROPERTY(config, EditAnywhere, Category = "Camera")
	float BobAmplitude = 2.2f;

	UPROPERTY(config, EditAnywhere, Category = "Camera")
	float BobFrequency = 1.8f;

	UPROPERTY(config, EditAnywhere, Category = "Camera")
	float BreathAmplitude = 0.35f;

	UPROPERTY(config, EditAnywhere, Category = "Camera")
	float LandingDipAmplitude = 9.0f;

	UPROPERTY(config, EditAnywhere, Category = "Camera")
	float LandingDipStiffness = 14.0f;

	UPROPERTY(config, EditAnywhere, Category = "Camera")
	float LandingDipDamping = 6.5f;

	UPROPERTY(config, EditAnywhere, Category = "Camera", meta = (ToolTip = "Lean roll in degrees"))
	float LeanRoll = 12.0f;

	UPROPERTY(config, EditAnywhere, Category = "Camera", meta = (ToolTip = "Lean lateral offset, cm"))
	float LeanOffset = 42.0f;

	UPROPERTY(config, EditAnywhere, Category = "Camera")
	float LeanSpeed = 9.0f;

	UPROPERTY(config, EditAnywhere, Category = "Camera")
	float SprintFOVAdd = 5.0f;

	UPROPERTY(config, EditAnywhere, Category = "Camera")
	float BaseFOV = 95.0f;

	// --- Flashlight ---
	UPROPERTY(config, EditAnywhere, Category = "Flashlight", meta = (ToolTip = "Battery percent per second while on (~11 min continuous)"))
	float BatteryDrainPerSecond = 0.15f;

	UPROPERTY(config, EditAnywhere, Category = "Flashlight", meta = (ToolTip = "Playtest-tuned up from 900: the beam must READ at range, not just at arm's length"))
	float FlashlightIntensityCandela = 1700.0f;

	UPROPERTY(config, EditAnywhere, Category = "Flashlight")
	float FlashlightInnerCone = 13.0f;

	UPROPERTY(config, EditAnywhere, Category = "Flashlight")
	float FlashlightOuterCone = 25.0f;

	UPROPERTY(config, EditAnywhere, Category = "Flashlight")
	float FlashlightRange = 3400.0f;

	UPROPERTY(config, EditAnywhere, Category = "Flashlight", meta = (ToolTip = "Beam dims below this battery percent"))
	float BatteryDimBelow = 20.0f;

	// --- Noise loudness (0-100 scale, ROADMAP 7.3) ---
	UPROPERTY(config, EditAnywhere, Category = "Noise")
	float NoiseSneakStep = 8.0f;

	UPROPERTY(config, EditAnywhere, Category = "Noise")
	float NoiseWalkStep = 25.0f;

	UPROPERTY(config, EditAnywhere, Category = "Noise")
	float NoiseSprintStep = 70.0f;

	UPROPERTY(config, EditAnywhere, Category = "Noise")
	float NoiseCrouchMultiplier = 0.6f;

	UPROPERTY(config, EditAnywhere, Category = "Noise")
	float NoiseLanding = 55.0f;

	UPROPERTY(config, EditAnywhere, Category = "Noise")
	float NoiseFlashlightClick = 6.0f;

	UPROPERTY(config, EditAnywhere, Category = "Noise")
	float NoiseVault = 30.0f;

	UPROPERTY(config, EditAnywhere, Category = "Noise")
	float NoiseDoorSlow = 12.0f;

	UPROPERTY(config, EditAnywhere, Category = "Noise")
	float NoiseDoorFast = 40.0f;

	UPROPERTY(config, EditAnywhere, Category = "Noise")
	float NoiseDoorSlam = 85.0f;

	UPROPERTY(config, EditAnywhere, Category = "Noise", meta = (ToolTip = "Stride length between footstep events, cm"))
	float FootstepStride = 170.0f;

	// --- Interaction ---
	UPROPERTY(config, EditAnywhere, Category = "Interaction")
	float InteractTraceDistance = 260.0f;

	UPROPERTY(config, EditAnywhere, Category = "Interaction", meta = (ToolTip = "Hold this long for the quiet/careful interaction"))
	float QuietHoldSeconds = 0.35f;

	// --- Vault ---
	UPROPERTY(config, EditAnywhere, Category = "Vault")
	float VaultMaxLedgeHeight = 140.0f;

	UPROPERTY(config, EditAnywhere, Category = "Vault")
	float VaultDuration = 0.35f;

	UPROPERTY(config, EditAnywhere, Category = "Vault")
	float VaultForwardReach = 90.0f;
};
