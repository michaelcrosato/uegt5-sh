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
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Perception/FCLightRegistry.h"
#include "AI/FCWatcher.h"
#include "Objectives/FCRunSubsystem.h"
#include "Player/FCCameraCraftComponent.h"
#include "UI/FCShellSubsystem.h"
#include "UObject/ConstructorHelpers.h"

static TAutoConsoleVariable<int32> CVarFCPlayerDebug(
	TEXT("fc.Player.Debug"),
	0,
	TEXT("1 = on-screen gait/stamina/battery readout."));

static TAutoConsoleVariable<float> CVarFCSensitivity(
	TEXT("fc.Input.Sensitivity"),
	1.0f,
	TEXT("Mouse look sensitivity multiplier (settings menu)."));

static TAutoConsoleVariable<float> CVarFCInvertY(
	TEXT("fc.Input.InvertY"),
	0.0f,
	TEXT("1 = invert look Y (settings menu)."));

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarFCDevMode(
	TEXT("fc.DevMode"),
	0,
	TEXT("Dev tools (F1): overlay + F3 ghost, F4 god, F5 condition, F6 win, ")
	TEXT("F7 kill, F8 spawn watcher, F9 debug draws. Never in shipping."));
#endif

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

	// The torch body: entirely BEHIND the light origin (a mesh over the
	// origin would block its own beam), casting no shadow of its own.
	TorchBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorchBody"));
	TorchBody->SetupAttachment(Camera);
	TorchBody->SetRelativeLocation(FVector(6.0f, 10.0f, -9.0f));
	TorchBody->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f)); // Z axis -> forward
	TorchBody->SetRelativeScale3D(FVector(0.03f, 0.03f, 0.11f)); // tip stays behind the beam origin
	TorchBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TorchBody->CastShadow = false;

	// The bodiless player still casts a real shadow (decision log #18):
	// a hidden cylinder that only the lighting can see.
	ShadowProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShadowProxy"));
	ShadowProxy->SetupAttachment(GetCapsuleComponent());
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		ShadowProxy->SetStaticMesh(CylinderMesh.Object);
		TorchBody->SetStaticMesh(CylinderMesh.Object);
	}
	ShadowProxy->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.72f));
	ShadowProxy->SetRelativeLocation(FVector(0.0f, 0.0f, -4.0f));
	ShadowProxy->SetHiddenInGame(true);
	ShadowProxy->CastShadow = true;
	ShadowProxy->bCastHiddenShadow = true;
	ShadowProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	bUseControllerRotationYaw = true;
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	// Crouch shrinks the capsule around its center; 55 keeps the eye well
	// above the sill line and the proxy shadow plausible.
	GetCharacterMovement()->SetCrouchedHalfHeight(55.0f);
	GetCharacterMovement()->JumpZVelocity = 430.0f;
	GetCharacterMovement()->AirControl = 0.12f;
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
	// Old incandescent torch, not studio white (flash-check audit): the warm
	// tint is what separates the beam from moonlight and blown highlights.
	Flashlight->SetLightColor(FLinearColor(1.0f, 0.88f, 0.72f));
	// The beam must be VISIBLE IN AIR (playtest: "only shows up close to a
	// wall") - strong volumetric scatter; the scenes carry volumetric fog.
	Flashlight->SetVolumetricScatteringIntensity(8.0f);

	// Dark rubberised torch body - readable, never brighter than the beam.
	if (UMaterialInterface* BulbBase = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* TorchMID = UMaterialInstanceDynamic::Create(BulbBase, this);
		TorchMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.03f, 0.03f, 0.035f));
		TorchBody->SetMaterial(0, TorchMID);
	}

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
		UEnhancedInputComponent* ShellInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);

		auto GetShell = [this]() -> UFCShellSubsystem*
		{
			return GetGameInstance() != nullptr
				? GetGameInstance()->GetSubsystem<UFCShellSubsystem>() : nullptr;
		};

		UInputAction* PauseAction = MakeAction(TEXT("IA_Pause"), EInputActionValueType::Boolean);
		PauseAction->bTriggerWhenPaused = true; // Escape must also UNpause
		MappingContext->MapKey(PauseAction, EKeys::Escape);
		ShellInput->BindActionValueLambda(PauseAction, ETriggerEvent::Started,
			[this, GetShell](const FInputActionValue&)
			{
				if (UFCShellSubsystem* Shell = GetShell())
				{
					if (Shell->IsMenuActive())
					{
						Shell->OnMenuBack(); // settings->back, intro->skip; title ignores
						return;
					}
				}
				if (HealthState != EFCHealthState::Dead)
				{
					if (APlayerController* PC = Cast<APlayerController>(GetController()))
					{
						PC->SetPause(!GetWorld()->IsPaused());
					}
				}
			});

		// Menu navigation (title / settings / intro): arrows + Enter, F to
		// confirm on the intro card. All fire while paused.
		auto BindMenuKey = [&](const TCHAR* Name, const FKey Key, TFunction<void(UFCShellSubsystem&)> Handler)
		{
			UInputAction* Action = MakeAction(Name, EInputActionValueType::Boolean);
			Action->bTriggerWhenPaused = true;
			MappingContext->MapKey(Action, Key);
			ShellInput->BindActionValueLambda(Action, ETriggerEvent::Started,
				[GetShell, Handler](const FInputActionValue&)
				{
					if (UFCShellSubsystem* Shell = GetShell())
					{
						if (Shell->IsMenuActive())
						{
							Handler(*Shell);
						}
					}
				});
		};
		BindMenuKey(TEXT("IA_MenuUp"), EKeys::Up, [](UFCShellSubsystem& Shell) { Shell.OnMenuUp(); });
		BindMenuKey(TEXT("IA_MenuDown"), EKeys::Down, [](UFCShellSubsystem& Shell) { Shell.OnMenuDown(); });
		BindMenuKey(TEXT("IA_MenuLeft"), EKeys::Left, [](UFCShellSubsystem& Shell) { Shell.OnMenuAdjust(-1); });
		BindMenuKey(TEXT("IA_MenuRight"), EKeys::Right, [](UFCShellSubsystem& Shell) { Shell.OnMenuAdjust(+1); });
		BindMenuKey(TEXT("IA_MenuConfirm"), EKeys::Enter, [](UFCShellSubsystem& Shell) { Shell.OnMenuConfirm(); });
		BindMenuKey(TEXT("IA_MenuConfirmF"), EKeys::F, [](UFCShellSubsystem& Shell)
		{
			if (Shell.GetState() == EFCShellState::Intro)
			{
				Shell.OnMenuConfirm(); // "[F] step into the street"
			}
		});

		// F2: settings from the pause card.
		UInputAction* SettingsAction = MakeAction(TEXT("IA_PauseSettings"), EInputActionValueType::Boolean);
		SettingsAction->bTriggerWhenPaused = true;
		MappingContext->MapKey(SettingsAction, EKeys::F2);
		ShellInput->BindActionValueLambda(SettingsAction, ETriggerEvent::Started,
			[this, GetShell](const FInputActionValue&)
			{
				if (UFCShellSubsystem* Shell = GetShell())
				{
					if (!Shell->IsMenuActive() && GetWorld()->IsPaused())
					{
						Shell->OpenSettingsFromPause();
					}
				}
			});

		// Shell verbs a packaged player needs (M10 dev shell):
		// F10 quits from anywhere; R after death retries THE SAME SEED
		// (the city subsystem stashes it across in-process restarts).
		UInputAction* QuitAction = MakeAction(TEXT("IA_Quit"), EInputActionValueType::Boolean);
		QuitAction->bTriggerWhenPaused = true;
		MappingContext->MapKey(QuitAction, EKeys::F10);
		ShellInput->BindActionValueLambda(QuitAction, ETriggerEvent::Started,
			[this](const FInputActionValue&)
			{
				if (APlayerController* PC = Cast<APlayerController>(GetController()))
				{
					PC->ConsoleCommand(TEXT("quit"));
				}
			});

#if !UE_BUILD_SHIPPING
		// --- Dev mode (playtest ask: the tools a dev wants on hand) ---
		// F1 toggles; the rest act only while fc.DevMode=1. Overlay: FCHUD.
		auto BindDevKey = [&](const TCHAR* Name, const FKey Key, TFunction<void()> Handler)
		{
			UInputAction* Action = MakeAction(Name, EInputActionValueType::Boolean);
			Action->bTriggerWhenPaused = true;
			MappingContext->MapKey(Action, Key);
			ShellInput->BindActionValueLambda(Action, ETriggerEvent::Started,
				[Handler](const FInputActionValue&) { Handler(); });
		};
		static const auto DevModeVar = IConsoleManager::Get().FindConsoleVariable(TEXT("fc.DevMode"));
		auto DevOn = []() { return DevModeVar != nullptr && DevModeVar->GetInt() != 0; };
		BindDevKey(TEXT("IA_DevToggle"), EKeys::F1, [this]()
		{
			if (DevModeVar != nullptr)
			{
				DevModeVar->Set(DevModeVar->GetInt() != 0 ? 0 : 1, ECVF_SetByConsole);
			}
		});
		BindDevKey(TEXT("IA_DevGhost"), EKeys::F3, [this, DevOn]() { if (DevOn()) { ToggleGhost(); } });
		BindDevKey(TEXT("IA_DevGod"), EKeys::F4, [this, DevOn]()
		{
			if (DevOn())
			{
				bGodMode = !bGodMode;
				UE_LOG(LogFootcandle, Display, TEXT("[FCDEV] god %s"), bGodMode ? TEXT("ON") : TEXT("OFF"));
			}
		});
		BindDevKey(TEXT("IA_DevCondition"), EKeys::F5, [this, DevOn]()
		{
			if (DevOn())
			{
				if (UFCRunSubsystem* Run = GetWorld()->GetSubsystem<UFCRunSubsystem>())
				{
					Run->NotifyConditionSatisfied(TEXT("dev F5"));
				}
			}
		});
		BindDevKey(TEXT("IA_DevWin"), EKeys::F6, [this, DevOn]()
		{
			if (DevOn())
			{
				if (UFCRunSubsystem* Run = GetWorld()->GetSubsystem<UFCRunSubsystem>())
				{
					Run->NotifyExtractionComplete();
				}
			}
		});
		BindDevKey(TEXT("IA_DevKill"), EKeys::F7, [this, DevOn]()
		{
			if (DevOn())
			{
				const bool bWasGod = bGodMode;
				bGodMode = false;
				ApplyHunterContact(TEXT("dev F7"));
				ApplyHunterContact(TEXT("You asked for this (F7)."));
				bGodMode = bWasGod;
			}
		});
		BindDevKey(TEXT("IA_DevSpawnWatcher"), EKeys::F8, [this, DevOn]()
		{
			if (DevOn())
			{
				const FVector SpawnPos = GetActorLocation() + Camera->GetForwardVector() * 1200.0f + FVector(0, 0, 30);
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
				GetWorld()->SpawnActor<AFCWatcher>(SpawnPos, (-Camera->GetForwardVector()).Rotation(), Params);
				UE_LOG(LogFootcandle, Display, TEXT("[FCDEV] watcher spawned ahead"));
			}
		});
		BindDevKey(TEXT("IA_DevDebugDraw"), EKeys::F9, [DevOn]()
		{
			if (DevOn())
			{
				static bool bDraws = false;
				bDraws = !bDraws;
				if (IConsoleVariable* NoiseVar = IConsoleManager::Get().FindConsoleVariable(TEXT("fc.Noise.Debug")))
				{
					NoiseVar->Set(bDraws ? 1 : 0, ECVF_SetByConsole);
				}
			}
		});
#endif

		UInputAction* RetryAction = MakeAction(TEXT("IA_Retry"), EInputActionValueType::Boolean);
		RetryAction->bTriggerWhenPaused = true;
		MappingContext->MapKey(RetryAction, EKeys::R);
		ShellInput->BindActionValueLambda(RetryAction, ETriggerEvent::Started,
			[this](const FInputActionValue&)
			{
				if (HealthState == EFCHealthState::Dead)
				{
					if (APlayerController* PC = Cast<APlayerController>(GetController()))
					{
						PC->RestartLevel();
					}
				}
			});
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
	if (bVaulting || HealthState == EFCHealthState::Dead)
	{
		return;
	}
	const FVector2D Axis = Value.Get<FVector2D>();
#if !UE_BUILD_SHIPPING
	if (bGhostMode)
	{
		// Ghost flies where the camera looks.
		AddMovementInput(Camera->GetForwardVector(), Axis.Y);
		AddMovementInput(Camera->GetRightVector(), Axis.X);
		return;
	}
#endif
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AFCPlayerCharacter::OnLook(const FInputActionValue& Value)
{
	if (HealthState == EFCHealthState::Dead)
	{
		return;
	}
	const float Sensitivity = FMath::Clamp(CVarFCSensitivity.GetValueOnGameThread(), 0.05f, 5.0f);
	const float YSign = CVarFCInvertY.GetValueOnGameThread() > 0.5f ? -1.0f : 1.0f;
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X * Sensitivity);
	AddControllerPitchInput(Axis.Y * Sensitivity * YSign);
}

void AFCPlayerCharacter::OnSprintStarted(const FInputActionValue&) { bWantsSprint = true; }
void AFCPlayerCharacter::OnSprintCompleted(const FInputActionValue&) { bWantsSprint = false; }
void AFCPlayerCharacter::OnSneakStarted(const FInputActionValue&) { bWantsSneak = true; }
void AFCPlayerCharacter::OnSneakCompleted(const FInputActionValue&) { bWantsSneak = false; }

void AFCPlayerCharacter::OnCrouchToggle(const FInputActionValue&)
{
	if (HealthState == EFCHealthState::Dead)
	{
		return;
	}
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	if (bIsCrouched)
	{
		UnCrouch();
		CameraCraft->SetEyeHeightOffset(0.0f);
	}
	else
	{
		// PLAYTEST BUG (director): the old offset subtracted the full eye
		// delta ON TOP of the capsule shrink - the camera went under the
		// floor. The capsule center already drops by (standing - crouched)
		// half-height; only the REMAINDER belongs on the camera.
		const float StandingHalf = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		Crouch();
		const float CrouchedHalf = GetCharacterMovement()->GetCrouchedHalfHeight();
		CameraCraft->SetEyeHeightOffset(
			(Tuning->CrouchedEyeHeight - CrouchedHalf) - (Tuning->EyeHeight - StandingHalf));
	}
}

void AFCPlayerCharacter::OnInteractPressed(const FInputActionValue&)
{
	if (HealthState != EFCHealthState::Dead)
	{
		Interaction->OnInteractPressed();
	}
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

#if !UE_BUILD_SHIPPING
void AFCPlayerCharacter::TestSetFlashlight(const bool bOn)
{
	bFlashlightOn = bOn;
	Flashlight->SetVisibility(bOn);
}
#endif

void AFCPlayerCharacter::OnVault(const FInputActionValue&)
{
	if (bVaulting || HealthState == EFCHealthState::Dead)
	{
		return;
	}
	// Space is contextual (playtest ask): a ledge or window in reach vaults;
	// open ground jumps. Jumping is loud on landing - already handled.
	if (!TryStartVault() && GetCharacterMovement()->IsMovingOnGround() && !bIsCrouched)
	{
		Jump();
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
#if !UE_BUILD_SHIPPING
	if (bGodMode)
	{
		return; // dev mode: untouchable
	}
#endif
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
		// Dead: gameplay verbs stop (handlers gate on HealthState) but input
		// stays live so [R] retry and [F10] quit still work on the card.
		HealthState = EFCHealthState::Dead;
		GetCharacterMovement()->DisableMovement();
		if (bFlashlightOn)
		{
			bFlashlightOn = false;
			Flashlight->SetVisibility(false);
		}
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
	// The beam origin sits INSIDE the hidden shadow proxy, which fully
	// eclipsed the flashlight's surface lighting (found by -fcflashcheck: a
	// lit torch, a pitch-black door). While the torch is on, your body stops
	// casting; the beam's world shadows are the point. Off, the proxy shadow
	// (decision #18) returns.
	ShadowProxy->SetCastShadow(!bFlashlightOn);
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

	// Low obstacle check (50cm: solidly below a 90cm window sill, so sill
	// walls register instead of the probe grazing the opening edge).
	FHitResult WaistHit;
	const FVector WaistStart = Feet + FVector(0, 0, 50.0f);
	if (!GetWorld()->LineTraceSingleByChannel(WaistHit, WaistStart,
		WaistStart + Forward * Tuning->VaultForwardReach, ECC_Visibility, Params))
	{
		UE_LOG(LogFootcandle, Verbose, TEXT("[FCVAULT] no low obstacle"));
		return false;
	}
	UE_LOG(LogFootcandle, Verbose, TEXT("[FCVAULT] low hit %s at %s"),
		WaistHit.GetActor() ? *WaistHit.GetActor()->GetName() : TEXT("?"),
		*WaistHit.ImpactPoint.ToCompactString());

	// Find the ledge top: trace down from above the obstacle.
	const FVector Above = WaistHit.ImpactPoint + Forward * 30.0f
		+ FVector(0, 0, Tuning->VaultMaxLedgeHeight + 50.0f - 60.0f + 60.0f);
	FHitResult TopHit;
	// Trace all the way down past foot level - a short trace stopped 2cm
	// above far-side floors and silently killed window detection.
	const float DownLength = (Above.Z - Feet.Z) + 60.0f;
	if (!GetWorld()->LineTraceSingleByChannel(TopHit, Above,
		Above - FVector(0, 0, DownLength), ECC_Visibility, Params))
	{
		UE_LOG(LogFootcandle, Verbose, TEXT("[FCVAULT] no ledge top under probe"));
		return TryStartWindowClimb(Forward, Feet, Params);
	}
	const float LedgeHeight = TopHit.ImpactPoint.Z - Feet.Z;
	if (LedgeHeight > Tuning->VaultMaxLedgeHeight || LedgeHeight < 30.0f)
	{
		// Too tall to vault, or the probe overshot a THIN wall and found the
		// far-side floor (a window reads exactly like that) - try climbing
		// through instead of giving up.
		return TryStartWindowClimb(Forward, Feet, Params);
	}

	// Headroom above the landing point.
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Landing = TopHit.ImpactPoint + FVector(0, 0, HalfHeight + 4.0f);
	FCollisionShape Capsule = FCollisionShape::MakeCapsule(
		GetCapsuleComponent()->GetScaledCapsuleRadius(), HalfHeight);
	if (GetWorld()->OverlapBlockingTestByChannel(Landing, FQuat::Identity, ECC_Pawn, Capsule, Params))
	{
		// Ledge blocked overhead - a window sill under a lintel does this.
		// Try the CLIMB-THROUGH (playtest ask: in and out of windows).
		return TryStartWindowClimb(Forward, Feet, Params);
	}

	bVaulting = true;
	VaultAlpha = 0.0f;
	VaultStart = GetActorLocation();
	VaultTarget = Landing;
	VaultApexBonus = 18.0f;
	Stamina -= Tuning->VaultCost;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	EmitPlayerNoise(Tuning->NoiseVault, TEXT("Noise.Source.Vault"));
	return true;
}

bool AFCPlayerCharacter::TryStartWindowClimb(const FVector& Forward, const FVector& Feet,
	const FCollisionQueryParams& Params)
{
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();

	// A window reads as: blocked low (the sill wall), CLEAR through the
	// opening band, and a floor on the far side of the wall.
	const float ThroughReach = Tuning->VaultForwardReach + 80.0f; // wall + margin
	FHitResult BandHit;
	const FVector BandStart = Feet + FVector(0, 0, 150.0f); // mid-window band
	if (GetWorld()->LineTraceSingleByChannel(BandHit, BandStart,
		BandStart + Forward * ThroughReach, ECC_Visibility, Params))
	{
		UE_LOG(LogFootcandle, Verbose, TEXT("[FCVAULT] window band blocked by %s at %s"),
			BandHit.GetActor() ? *BandHit.GetActor()->GetName() : TEXT("?"),
			*BandHit.ImpactPoint.ToCompactString());
		return false; // no opening at window height
	}

	// Far-side floor.
	const FVector FarTop = BandStart + Forward * ThroughReach;
	FHitResult FloorHit;
	if (!GetWorld()->LineTraceSingleByChannel(FloorHit, FarTop,
		FarTop - FVector(0, 0, 500.0f), ECC_Visibility, Params))
	{
		UE_LOG(LogFootcandle, Verbose, TEXT("[FCVAULT] no far floor under %s"), *FarTop.ToCompactString());
		return false; // nothing to land on (do not dive out of high windows blindly)
	}

	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Landing = FloorHit.ImpactPoint + FVector(0, 0, HalfHeight + 4.0f);
	FCollisionShape Capsule = FCollisionShape::MakeCapsule(
		GetCapsuleComponent()->GetScaledCapsuleRadius(), HalfHeight);
	if (GetWorld()->OverlapBlockingTestByChannel(Landing, FQuat::Identity, ECC_Pawn, Capsule, Params))
	{
		return false;
	}
	if (Stamina < Tuning->VaultCost)
	{
		return false;
	}

	bVaulting = true;
	VaultAlpha = 0.0f;
	VaultStart = GetActorLocation();
	VaultTarget = Landing;
	// Arc the camera up through the opening - reads as pulling yourself over
	// the sill (collision is ignored during the arc; the shape is the feel).
	VaultApexBonus = FMath::Max(BandStart.Z + 20.0f - FMath::Max(VaultStart.Z, VaultTarget.Z), 24.0f);
	Stamina -= Tuning->VaultCost;
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	EmitPlayerNoise(Tuning->NoiseVault + 5.0f, TEXT("Noise.Source.WindowClimb"));
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
	// Camera-space parabola: the arc height scales for window climbs.
	Position.Z += FMath::Sin(Eased * PI) * VaultApexBonus;
	SetActorLocation(Position, false, nullptr, ETeleportType::TeleportPhysics);

	if (VaultAlpha >= 1.0f)
	{
		bVaulting = false;
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

#if !UE_BUILD_SHIPPING
void AFCPlayerCharacter::TestToggleCrouch()
{
	OnCrouchToggle(FInputActionValue());
}

FVector AFCPlayerCharacter::TestGetCameraLocation() const
{
	return Camera->GetComponentLocation();
}

void AFCPlayerCharacter::ToggleGhost()
{
	bGhostMode = !bGhostMode;
	if (bGhostMode)
	{
		GetCharacterMovement()->SetMovementMode(MOVE_Flying);
		GetCharacterMovement()->MaxFlySpeed = 1400.0f;
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	}
	else
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCDEV] ghost %s"), bGhostMode ? TEXT("ON") : TEXT("OFF"));
}
#endif

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
