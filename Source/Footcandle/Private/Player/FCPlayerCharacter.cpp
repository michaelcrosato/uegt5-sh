#include "Player/FCPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FCTuningSettings.h"
#include "Footcandle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Interaction/FCInteractionComponent.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Perception/FCLightRegistry.h"
#include "Player/FCCameraCraftComponent.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<int32> CVarFCPlayerDebug(
	TEXT("fc.Player.Debug"),
	0,
	TEXT("1 = on-screen gait/stamina/battery readout."));

AFCPlayerCharacter::AFCPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 90.0f);

	CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
	CameraRoot->SetupAttachment(GetCapsuleComponent());
	CameraRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 75.0f)); // eye 165 over ground

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraRoot);
	Camera->bUsePawnControlRotation = true;

	CameraCraft = CreateDefaultSubobject<UFCCameraCraftComponent>(TEXT("CameraCraft"));
	Interaction = CreateDefaultSubobject<UFCInteractionComponent>(TEXT("Interaction"));

	Flashlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Flashlight"));
	Flashlight->SetupAttachment(Camera);
	Flashlight->SetRelativeLocation(FVector(12.0f, 9.0f, -7.0f));
	Flashlight->SetVisibility(false);
	Flashlight->SetMobility(EComponentMobility::Movable);

	// The bodiless player still casts a real shadow (decision log #18):
	// a hidden cylinder that only the lighting can see.
	ShadowProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowProxy"));
	ShadowProxy->SetupAttachment(GetCapsuleComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		ShadowProxy->SetStaticMesh(CylinderMesh.Object);
	}
	ShadowProxy->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.72f));
	ShadowProxy->SetRelativeLocation(FVector(0.0f, 0.0f, -4.0f));
	ShadowProxy->SetHiddenInGame(true);
	ShadowProxy->CastShadow = true;
	ShadowProxy->bCastHiddenShadow = true;
	ShadowProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bUseControllerRotationYaw = true;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
}

void AFCPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	Stamina = Tuning->StaminaMax;
	GetCharacterMovement()->MaxWalkSpeed = Tuning->WalkSpeed;
	GetCharacterMovement()->MaxWalkSpeedCrouched = Tuning->CrouchSpeed;
	Camera->SetFieldOfView(Tuning->BaseFOV);

	Flashlight->SetIntensityUnits(ELightUnits::Candelas);
	Flashlight->SetIntensity(Tuning->FlashlightIntensityCandela);
	Flashlight->SetInnerConeAngle(Tuning->FlashlightInnerCone);
	Flashlight->SetOuterConeAngle(Tuning->FlashlightOuterCone);
	Flashlight->SetAttenuationRadius(Tuning->FlashlightRange);
	Flashlight->SetVolumetricScatteringIntensity(2.0f);

	CameraCraft->SetTargets(CameraRoot, Camera);

	// The flashlight is a real light to the perception model too (ROADMAP 8.3).
	if (UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
	{
		Registry->RegisterLight(Flashlight);
	}
}

void AFCPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Procedural Enhanced Input (ADR-0005): actions and mappings authored
	// here, in reviewable code, not in binary assets.
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_FC"));

	auto MakeAction = [this](const TCHAR* Name, const EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};

	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	SprintAction = MakeAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	SneakAction = MakeAction(TEXT("IA_Sneak"), EInputActionValueType::Boolean);
	CrouchAction = MakeAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	LeanAction = MakeAction(TEXT("IA_Lean"), EInputActionValueType::Axis1D);
	InteractAction = MakeAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	FlashlightAction = MakeAction(TEXT("IA_Flashlight"), EInputActionValueType::Boolean);
	VaultAction = MakeAction(TEXT("IA_Vault"), EInputActionValueType::Boolean);
	ListenAction = MakeAction(TEXT("IA_Listen"), EInputActionValueType::Boolean);

	// WASD -> Axis2D (X = right, Y = forward).
	{
		FEnhancedActionKeyMapping& W = MappingContext->MapKey(MoveAction, EKeys::W);
		W.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		FEnhancedActionKeyMapping& S = MappingContext->MapKey(MoveAction, EKeys::S);
		S.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		S.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		MappingContext->MapKey(MoveAction, EKeys::D);
		FEnhancedActionKeyMapping& A = MappingContext->MapKey(MoveAction, EKeys::A);
		A.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	// Mouse look; negate Y so mouse-up looks up.
	{
		FEnhancedActionKeyMapping& Mouse = MappingContext->MapKey(LookAction, EKeys::Mouse2D);
		UInputModifierNegate* NegateY = NewObject<UInputModifierNegate>(this);
		NegateY->bX = false;
		NegateY->bY = true;
		NegateY->bZ = false;
		Mouse.Modifiers.Add(NegateY);
	}
	MappingContext->MapKey(SprintAction, EKeys::LeftShift);
	MappingContext->MapKey(SneakAction, EKeys::LeftControl);
	MappingContext->MapKey(CrouchAction, EKeys::C);
	{
		// Q = lean left (-1), E = lean right (+1).
		FEnhancedActionKeyMapping& Q = MappingContext->MapKey(LeanAction, EKeys::Q);
		Q.Modifiers.Add(NewObject<UInputModifierNegate>(this));
		MappingContext->MapKey(LeanAction, EKeys::E);
	}
	MappingContext->MapKey(InteractAction, EKeys::F);
	MappingContext->MapKey(FlashlightAction, EKeys::T);
	MappingContext->MapKey(VaultAction, EKeys::SpaceBar);
	MappingContext->MapKey(ListenAction, EKeys::LeftAlt);
	{
		UInputAction* PauseAction = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean);
		PauseAction->bTriggerWhenPaused = true; // Escape must also UNpause
		MappingContext->MapKey(PauseAction, EKeys::Escape);
		if (UEnhancedInputComponent* PauseInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
		{
			PauseInput->BindActionValueLambda(PauseAction, ETriggerEvent::Started,
				[this](const FInputActionValue&)
				{
					if (APlayerController* PC = Cast<APlayerController>(GetController()))
					{
						PC->SetPause(!GetWorld()->IsPaused());
					}
				});
		}
	}

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	UEnhancedInputComponent* Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);
	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFCPlayerCharacter::OnMove);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFCPlayerCharacter::OnLook);
	Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AFCPlayerCharacter::OnSprintStarted);
	Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AFCPlayerCharacter::OnSprintCompleted);
	Input->BindAction(SneakAction, ETriggerEvent::Started, this, &AFCPlayerCharacter::OnSneakStarted);
	Input->BindAction(SneakAction, ETriggerEvent::Completed, this, &AFCPlayerCharacter::OnSneakCompleted);
	Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AFCPlayerCharacter::OnCrouchToggle);
	Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AFCPlayerCharacter::OnInteractPressed);
	Input->BindAction(InteractAction, ETriggerEvent::Completed, this, &AFCPlayerCharacter::OnInteractReleased);
	Input->BindAction(LeanAction, ETriggerEvent::Triggered, this, &AFCPlayerCharacter::OnLean);
	Input->BindAction(LeanAction, ETriggerEvent::Completed, this, &AFCPlayerCharacter::OnLeanCompleted);
	Input->BindAction(FlashlightAction, ETriggerEvent::Started, this, &AFCPlayerCharacter::OnFlashlightToggle);
	Input->BindAction(VaultAction, ETriggerEvent::Started, this, &AFCPlayerCharacter::OnVault);
	Input->BindAction(ListenAction, ETriggerEvent::Started, this, &AFCPlayerCharacter::OnListenStarted);
	Input->BindAction(ListenAction, ETriggerEvent::Completed, this, &AFCPlayerCharacter::OnListenCompleted);
}

void AFCPlayerCharacter::OnMove(const FInputActionValue& Value)
{
	if (bVaulting)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AFCPlayerCharacter::OnLook(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AFCPlayerCharacter::OnSprintStarted(const FInputActionValue&) { bWantsSprint = true; }
void AFCPlayerCharacter::OnSprintCompleted(const FInputActionValue&) { bWantsSprint = false; }
void AFCPlayerCharacter::OnSneakStarted(const FInputActionValue&) { bWantsSneak = true; }
void AFCPlayerCharacter::OnSneakCompleted(const FInputActionValue&) { bWantsSneak = false; }

void AFCPlayerCharacter::OnCrouchToggle(const FInputActionValue&)
{
	if (bIsCrouched)
	{
		UnCrouch();
		CameraCraft->SetEyeHeightOffset(0.0f);
	}
	else
	{
		Crouch();
		const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
		CameraCraft->SetEyeHeightOffset(Tuning->CrouchedEyeHeight - Tuning->EyeHeight);
	}
}

void AFCPlayerCharacter::OnInteractPressed(const FInputActionValue&)
{
	Interaction->OnInteractPressed();
}

void AFCPlayerCharacter::OnInteractReleased(const FInputActionValue&)
{
	Interaction->OnInteractReleased();
}

void AFCPlayerCharacter::OnLean(const FInputActionValue& Value)
{
	CameraCraft->SetLeanAxis(Value.Get<float>());
}

void AFCPlayerCharacter::OnLeanCompleted(const FInputActionValue&)
{
	CameraCraft->SetLeanAxis(0.0f);
}

void AFCPlayerCharacter::OnFlashlightToggle(const FInputActionValue&)
{
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	if (!bFlashlightOn && Battery <= 0.0f)
	{
		return; // dead battery: the click still happens below? No - no power, no click-on.
	}
	bFlashlightOn = !bFlashlightOn;
	Flashlight->SetVisibility(bFlashlightOn);
	EmitPlayerNoise(Tuning->NoiseFlashlightClick, TEXT("Noise.Source.FlashlightClick"));
}

void AFCPlayerCharacter::OnVault(const FInputActionValue&)
{
	if (!bVaulting)
	{
		TryStartVault();
	}
}

void AFCPlayerCharacter::OnListenStarted(const FInputActionValue&) { bListening = true; }
void AFCPlayerCharacter::OnListenCompleted(const FInputActionValue&) { bListening = false; }

void AFCPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	const float ImpactSpeed = FMath::Abs(GetVelocity().Z);
	const float Normalized = FMath::Clamp((ImpactSpeed - 200.0f) / 800.0f, 0.05f, 1.0f);
	CameraCraft->NotifyLanded(Normalized);
	if (ImpactSpeed > 250.0f)
	{
		EmitPlayerNoise(Tuning->NoiseLanding * Normalized, TEXT("Noise.Source.Landing"));
	}
}

float AFCPlayerCharacter::GetPassiveNoiseFloor() const
{
	// ROADMAP 7.3: still+listening 0; still 3; critical 22; exhausted +20.
	const bool bStill = GetVelocity().SizeSquared() < 25.0f;
	float Floor = bStill ? (bListening ? 0.0f : 3.0f) : 0.0f;
	if (HealthState == EFCHealthState::Critical)
	{
		Floor += 22.0f; // the hurt-spiral: wounded breathing is loud
	}
	if (GetWorld()->GetTimeSeconds() < ExhaustedUntil)
	{
		Floor += 20.0f;
	}
	return Floor;
}

void AFCPlayerCharacter::ApplyHunterContact(const FString& AttributionSentence)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastContactTime < 1.5f || HealthState == EFCHealthState::Dead)
	{
		return; // one strike per grab; no double-tap frames
	}
	LastContactTime = Now;

	if (HealthState == EFCHealthState::Fine)
	{
		HealthState = EFCHealthState::Critical;
		CameraCraft->NotifyLanded(1.0f); // the hit reads through the camera
		UE_LOG(LogFootcandle, Warning, TEXT("[FCPLAYER] CRITICAL - escape window open"));
	}
	else
	{
		HealthState = EFCHealthState::Dead;
		DisableInput(Cast<APlayerController>(GetController()));
		UE_LOG(LogFootcandle, Error, TEXT("[FCPLAYER] DEAD - %s"), *AttributionSentence);
	}
}

void AFCPlayerCharacter::UpdateGaitAndStamina(const float DeltaSeconds)
{
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float Now = GetWorld()->GetTimeSeconds();
	const float Speed2D = GetVelocity().Size2D();

	const bool bCanSprint = Stamina > 0.0f && !bIsCrouched && !bListening;
	if (bWantsSprint && bCanSprint && Speed2D > 10.0f)
	{
		Gait = EFCGait::Sprint;
	}
	else if (bWantsSneak || bListening)
	{
		Gait = EFCGait::Sneak;
	}
	else
	{
		Gait = EFCGait::Walk;
	}

	switch (Gait)
	{
	case EFCGait::Sprint: Movement->MaxWalkSpeed = Tuning->SprintSpeed; break;
	case EFCGait::Sneak: Movement->MaxWalkSpeed = Tuning->SneakSpeed; break;
	default: Movement->MaxWalkSpeed = Tuning->WalkSpeed; break;
	}

	if (Gait == EFCGait::Sprint && Speed2D > 10.0f)
	{
		Stamina -= Tuning->SprintDrainPerSecond * DeltaSeconds;
		RegenSuppressedUntil = Now + Tuning->RegenSuppressSeconds;
		if (Stamina <= 0.0f)
		{
			Stamina = 0.0f;
			ExhaustedUntil = Now + 8.0f;
			bWantsSprint = false;
		}
		else if (Stamina < Tuning->ExhaustedThreshold)
		{
			ExhaustedUntil = Now + 8.0f;
		}
	}
	else if (Now >= RegenSuppressedUntil)
	{
		const float Regen = Speed2D > 10.0f ? Tuning->RegenPerSecondMoving : Tuning->RegenPerSecondStill;
		Stamina = FMath::Min(Stamina + Regen * DeltaSeconds, Tuning->StaminaMax);
	}

	CameraCraft->SetGaitState(Speed2D, Gait == EFCGait::Sprint, bIsCrouched, bListening);
}

void AFCPlayerCharacter::UpdateFootsteps(const float DeltaSeconds)
{
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	const float Speed2D = GetVelocity().Size2D();
	if (GetCharacterMovement()->IsMovingOnGround() && Speed2D > 20.0f)
	{
		FootstepAccumulator += Speed2D * DeltaSeconds;
		if (FootstepAccumulator >= Tuning->FootstepStride)
		{
			FootstepAccumulator = 0.0f;
			float Loudness = Tuning->NoiseWalkStep;
			switch (Gait)
			{
			case EFCGait::Sneak: Loudness = Tuning->NoiseSneakStep; break;
			case EFCGait::Sprint: Loudness = Tuning->NoiseSprintStep; break;
			default: break;
			}
			if (bIsCrouched)
			{
				Loudness *= Tuning->NoiseCrouchMultiplier;
			}
			EmitPlayerNoise(Loudness, TEXT("Noise.Source.Footstep"));
		}
	}
}

void AFCPlayerCharacter::UpdateFlashlight(const float DeltaSeconds)
{
	// The beam is a pointing finger: register it with the perception model
	// whether on or off (off clears it).
	if (UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
	{
		Registry->SetActiveBeam(this, Flashlight->GetComponentLocation(),
			Camera->GetForwardVector(), UFCTuningSettings::Get()->FlashlightRange, bFlashlightOn);
	}
	if (!bFlashlightOn)
	{
		return;
	}
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	Battery = FMath::Max(Battery - Tuning->BatteryDrainPerSecond * DeltaSeconds, 0.0f);
	if (Battery <= 0.0f)
	{
		bFlashlightOn = false;
		Flashlight->SetVisibility(false);
		return;
	}
	// Diegetic battery read: the beam dims as the cell dies (ROADMAP 11.2).
	const float DimAlpha = FMath::Clamp(Battery / Tuning->BatteryDimBelow, 0.25f, 1.0f);
	Flashlight->SetIntensity(Tuning->FlashlightIntensityCandela * DimAlpha);
}

bool AFCPlayerCharacter::TryStartVault()
{
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	if (Stamina < Tuning->VaultCost || !GetCharacterMovement()->IsMovingOnGround())
	{
		return false;
	}

	const FVector Forward = GetActorForwardVector();
	const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FCVault), false, this);

	// Waist-height obstacle check.
	FHitResult WaistHit;
	const FVector WaistStart = Feet + FVector(0, 0, 60.0f);
	if (!GetWorld()->LineTraceSingleByChannel(WaistHit, WaistStart,
		WaistStart + Forward * Tuning->VaultForwardReach, ECC_Visibility, Params))
	{
		return false;
	}

	// Find the ledge top: trace down from above the obstacle.
	const FVector Above = WaistHit.ImpactPoint + Forward * 30.0f
		+ FVector(0, 0, Tuning->VaultMaxLedgeHeight + 50.0f - 60.0f + 60.0f);
	FHitResult TopHit;
	if (!GetWorld()->LineTraceSingleByChannel(TopHit, Above,
		Above - FVector(0, 0, Tuning->VaultMaxLedgeHeight + 100.0f), ECC_Visibility, Params))
	{
		return false;
	}
	const float LedgeHeight = TopHit.ImpactPoint.Z - Feet.Z;
	if (LedgeHeight > Tuning->VaultMaxLedgeHeight || LedgeHeight < 30.0f)
	{
		return false;
	}

	// Headroom above the landing point.
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Landing = TopHit.ImpactPoint + FVector(0, 0, HalfHeight + 4.0f);
	FCollisionShape Capsule = FCollisionShape::MakeCapsule(
		GetCapsuleComponent()->GetScaledCapsuleRadius(), HalfHeight);
	if (GetWorld()->OverlapBlockingTestByChannel(Landing, FQuat::Identity, ECC_Pawn, Capsule, Params))
	{
		return false;
	}

	bVaulting = true;
	VaultAlpha = 0.0f;
	VaultStart = GetActorLocation();
	VaultTarget = Landing;
	Stamina -= Tuning->VaultCost;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	EmitPlayerNoise(Tuning->NoiseVault, TEXT("Noise.Source.Vault"));
	return true;
}

void AFCPlayerCharacter::UpdateVault(const float DeltaSeconds)
{
	if (!bVaulting)
	{
		return;
	}
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	VaultAlpha = FMath::Min(VaultAlpha + DeltaSeconds / Tuning->VaultDuration, 1.0f);
	const float Eased = FMath::InterpEaseInOut(0.0f, 1.0f, VaultAlpha, 2.0f);
	FVector Position = FMath::Lerp(VaultStart, VaultTarget, Eased);
	// Camera-space parabola: a small arc so the move reads as a push-up-over.
	Position.Z += FMath::Sin(Eased * PI) * 18.0f;
	SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);

	if (VaultAlpha >= 1.0f)
	{
		bVaulting = false;
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

void AFCPlayerCharacter::EmitPlayerNoise(const float Loudness, const FName Tag)
{
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->EmitNoise(GetActorLocation(), Loudness, Tag, this);
	}
}

void AFCPlayerCharacter::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateGaitAndStamina(DeltaSeconds);
	UpdateFootsteps(DeltaSeconds);
	UpdateFlashlight(DeltaSeconds);
	UpdateVault(DeltaSeconds);

	if (CVarFCPlayerDebug.GetValueOnGameThread() != 0 && GEngine != nullptr)
	{
		GEngine->AddOnScreenDebugMessage(1, 0.0f, FColor::Yellow,
			FString::Printf(TEXT("Gait=%d Stamina=%.0f Battery=%.0f Listen=%d NoiseFloor=%.0f"),
				static_cast<int32>(Gait), Stamina, Battery, bListening ? 1 : 0,
				GetPassiveNoiseFloor()));
	}
}
