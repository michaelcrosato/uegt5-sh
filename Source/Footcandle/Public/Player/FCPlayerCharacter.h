#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "FCPlayerCharacter.generated.h"

class UCameraComponent;
class UFCCameraCraftComponent;
class UFCInteractionComponent;
class UInputAction;
class UInputMappingContext;
class USpotLightComponent;
class UStaticMeshComponent;
struct FInputActionValue;

UENUM()
enum class EFCGait : uint8
{
	Sneak,
	Walk,
	Sprint,
};

// Two-strike contact model (ROADMAP 4.5, decision #8).
UENUM()
enum class EFCHealthState : uint8
{
	Fine,
	Critical,
	Dead,
};

// The FOOTCANDLE player (ROADMAP 4.6): no visible body, arms-later viewmodel,
// full camera craft, and a hidden shadow-proxy so the player casts a real
// shadow past every lamp (decision log #18). Input is built procedurally in
// C++ - no binary input assets (ADR-0005).
// Binds: WASD move, mouse look, Shift sprint, Ctrl sneak, C crouch, Q/E lean,
// F interact (hold = quiet), T flashlight, G throw, Space vault, Alt listen.
UCLASS()
class FOOTCANDLE_API AFCPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AFCPlayerCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;

	float GetStamina() const { return Stamina; }
	float GetBattery() const { return Battery; }
	bool IsListening() const { return bListening; }
	EFCGait GetGait() const { return Gait; }

	// Current passive noise floor (ROADMAP 7.3): 0 while still+listening.
	float GetPassiveNoiseFloor() const;

	// Hunter contact: first landed strike wounds (Critical + escape window),
	// second kills. The death line NAMES the system that won (decision #8).
	void ApplyHunterContact(const FString& AttributionSentence);

	// Save/load restore (UFCSaveSubsystem only).
	void RestoreFromSave(const float InBattery, const float InStamina)
	{
		Battery = InBattery;
		Stamina = InStamina;
	}
	EFCHealthState GetHealthState() const { return HealthState; }
	bool IsFlashlightOn() const { return bFlashlightOn; }

#if !UE_BUILD_SHIPPING
	// Scripted-smoke hooks (FCM1Smoke) - never compiled into shipping.
	bool TestVault() { return TryStartVault(); }
	void TestSetSprint(const bool bSprint) { bWantsSprint = bSprint; }
	void TestSetFlashlight(bool bOn);

	// Dev mode (fc.DevMode, F1): tools a dev wants on hand.
	bool bGodMode = false;
	bool bGhostMode = false;
	void ToggleGhost();
	void TestToggleCrouch();
	FVector TestGetCameraLocation() const;
#endif

protected:
	// --- Components ---
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<USceneComponent> CameraRoot;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UFCCameraCraftComponent> CameraCraft;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<USpotLightComponent> Flashlight;

	// The torch is a visible object in your hand (decision #29), not a
	// disembodied cone - a small dark cylinder at the beam's root.
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> TorchBody;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> ShadowProxy;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UFCInteractionComponent> Interaction;

	// --- Input plumbing (created procedurally) ---
	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;
	UPROPERTY()
	TObjectPtr<UInputAction> SprintAction;
	UPROPERTY()
	TObjectPtr<UInputAction> SneakAction;
	UPROPERTY()
	TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY()
	TObjectPtr<UInputAction> LeanAction;
	UPROPERTY()
	TObjectPtr<UInputAction> InteractAction;
	UPROPERTY()
	TObjectPtr<UInputAction> FlashlightAction;
	UPROPERTY()
	TObjectPtr<UInputAction> VaultAction;
	UPROPERTY()
	TObjectPtr<UInputAction> ListenAction;

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnSprintStarted(const FInputActionValue& Value);
	void OnSprintCompleted(const FInputActionValue& Value);
	void OnSneakStarted(const FInputActionValue& Value);
	void OnSneakCompleted(const FInputActionValue& Value);
	void OnCrouchToggle(const FInputActionValue& Value);
	void OnInteractPressed(const FInputActionValue& Value);
	void OnInteractReleased(const FInputActionValue& Value);
	void OnLean(const FInputActionValue& Value);
	void OnLeanCompleted(const FInputActionValue& Value);
	void OnFlashlightToggle(const FInputActionValue& Value);
	void OnVault(const FInputActionValue& Value);
	void OnListenStarted(const FInputActionValue& Value);
	void OnListenCompleted(const FInputActionValue& Value);

private:
	void UpdateGaitAndStamina(float DeltaSeconds);
	void UpdateFootsteps(float DeltaSeconds);
	void UpdateFlashlight(float DeltaSeconds);
	void UpdateVault(float DeltaSeconds);
	bool TryStartVault();
	bool TryStartWindowClimb(const FVector& Forward, const FVector& Feet,
		const struct FCollisionQueryParams& Params);
	void EmitPlayerNoise(float Loudness, FName Tag);

	EFCGait Gait = EFCGait::Walk;
	EFCHealthState HealthState = EFCHealthState::Fine;
	float LastContactTime = -100.0f;
	bool bWantsSprint = false;
	bool bWantsSneak = false;
	bool bListening = false;
	float Stamina = 100.0f;
	float RegenSuppressedUntil = 0.0f;
	float ExhaustedUntil = 0.0f;
	float Battery = 100.0f;
	bool bFlashlightOn = false;
	float FootstepAccumulator = 0.0f;

	// Vault state.
	bool bVaulting = false;
	float VaultAlpha = 0.0f;
	float VaultApexBonus = 18.0f;
	FVector VaultStart = FVector::ZeroVector;
	FVector VaultTarget = FVector::ZeroVector;
};
