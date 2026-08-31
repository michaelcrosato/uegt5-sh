#include "AI/FCWatcher.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "NavigationInvokerComponent.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Perception/FCLightRegistry.h"
#include "Player/FCPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* StateName(const EFCWatcherState State)
	{
		switch (State)
		{
		case EFCWatcherState::Idle: return TEXT("Idle");
		case EFCWatcherState::Patrol: return TEXT("Patrol");
		case EFCWatcherState::Suspicious: return TEXT("Suspicious");
		case EFCWatcherState::Investigate: return TEXT("Investigate");
		case EFCWatcherState::Hunt: return TEXT("Hunt");
		case EFCWatcherState::Search: return TEXT("Search");
		default: return TEXT("?");
		}
	}
}

AFCWatcher::AFCWatcher()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(45.0f, 135.0f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Tall, thin, hard-edged. It glides; nothing on it ever plays a clip.
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	if (CylinderMesh.Succeeded())
	{
		Body->SetStaticMesh(CylinderMesh.Object);
	}
	Body->SetRelativeScale3D(FVector(0.55f, 0.55f, 2.55f));
	Body->SetRelativeLocation(FVector(0, 0, -8.0f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(Capsule);
	if (CubeMesh.Succeeded())
	{
		Head->SetStaticMesh(CubeMesh.Object);
	}
	Head->SetRelativeScale3D(FVector(0.36f, 0.44f, 0.34f));
	Head->SetRelativeLocation(FVector(0, 0, 128.0f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The awareness broadcast: the player must always know WHETHER it knows.
	EyeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLight"));
	EyeLight->SetupAttachment(Head);
	EyeLight->SetRelativeLocation(FVector(24.0f, 0, 0));
	EyeLight->SetIntensityUnits(ELightUnits::Candelas);
	EyeLight->SetIntensity(2.0f);
	EyeLight->SetLightColor(FLinearColor(0.3f, 0.4f, 1.0f));
	EyeLight->SetAttenuationRadius(700.0f);
	EyeLight->SetMobility(EComponentMobility::Movable);

	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->MaxSpeed = GlideSpeed;
	Movement->Acceleration = 900.0f;
	Movement->Deceleration = 1400.0f;

	NavInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvoker"));

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AFCWatcher::BeginPlay()
{
	Super::BeginPlay();

	Movement->MaxSpeed = GlideSpeed;
	InterestPoint = GetActorLocation() + GetActorForwardVector() * 500.0f;

	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		NoiseHandle = Noise->OnNoiseEmitted.AddUObject(this, &AFCWatcher::OnNoise);
	}
	// Its own eye is a real light in the world - the shadow it casts and the
	// glow that precedes it around corners are honest (P4).
	if (UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
	{
		Registry->RegisterLight(EyeLight);
	}

	SetState(PatrolPoints.Num() > 0 ? EFCWatcherState::Patrol : EFCWatcherState::Idle, TEXT("begin play"));
}

#if !UE_BUILD_SHIPPING
void AFCWatcher::TestResetToIdle()
{
	PatrolPoints.Empty();
	DetectionMeter = 0.0f;
	bHasLastKnownPlayerPos = false;
	SetState(EFCWatcherState::Idle, TEXT("test reset"));
	InterestPoint = GetActorLocation() + GetActorForwardVector() * 500.0f;
}
#endif

void AFCWatcher::SetState(const EFCWatcherState NewState, const TCHAR* Reason)
{
	if (State == NewState)
	{
		return;
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCWATCHER] %s -> %s (%s)"),
		StateName(State), StateName(NewState), Reason);
	State = NewState;
	StateTimer = 0.0f;
}

void AFCWatcher::OnNoise(const FFCNoiseEvent& Event)
{
	if (Event.Instigator == this)
	{
		return;
	}
	UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>();
	if (Noise == nullptr)
	{
		return;
	}
	const float Perceived = Noise->PerceivedLoudnessAt(Event, GetActorLocation());
	if (Perceived < HearingThreshold)
	{
		return;
	}

	// Positional error proportional to faintness: it investigates NEAR the
	// noise - exactness reads as omniscience (ROADMAP 8.3, AI-01).
	const float Margin = FMath::Clamp((Perceived - HearingThreshold) / 40.0f, 0.0f, 1.0f);
	const float ErrorRadius = FMath::Lerp(450.0f, 60.0f, Margin);
	const float ErrorAngle = FMath::Frac(Event.Timestamp * 7.31f) * 2.0f * PI; // event-derived, not RNG
	InterestPoint = Event.Origin + FVector(FMath::Cos(ErrorAngle), FMath::Sin(ErrorAngle), 0) * ErrorRadius;

	if (State != EFCWatcherState::Hunt)
	{
		SetState(EFCWatcherState::Investigate,
			*FString::Printf(TEXT("heard %s at %.0f"), *Event.SourceTag.ToString(), Perceived));
	}
}

float AFCWatcher::ComputePlayerVisibility(const AFCPlayerCharacter* Player) const
{
	const FVector MyEye = Head->GetComponentLocation();
	const FVector ToPlayer = Player->GetActorLocation() - MyEye;
	const float Dist = ToPlayer.Size();
	if (Dist > SightRange)
	{
		return 0.0f;
	}
	// View cone.
	const float CosAngle = FVector::DotProduct(GetActorForwardVector(), ToPlayer.GetSafeNormal());
	if (CosAngle < FMath::Cos(FMath::DegreesToRadians(SightConeHalfAngleDeg)))
	{
		return 0.0f;
	}

	// 3-point trace: head, chest, feet - partial cover gives partial rate.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FCWatcherSight), true, this);
	Params.AddIgnoredActor(Player);
	int32 VisiblePoints = 0;
	const float Offsets[] = { 70.0f, 0.0f, -80.0f };
	for (const float Offset : Offsets)
	{
		FHitResult Hit;
		const FVector Target = Player->GetActorLocation() + FVector(0, 0, Offset);
		if (!GetWorld()->LineTraceSingleByChannel(Hit, MyEye, Target, ECC_Visibility, Params))
		{
			++VisiblePoints;
		}
	}
	if (VisiblePoints == 0)
	{
		return 0.0f;
	}

	// Light-level multiplier from the registry - the honest, CPU-side model
	// (never the renderer). Darkness floor: never invisible at arm's length.
	const UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>();
	float Illumination = Registry != nullptr
		? Registry->SampleIllumination(Player->GetActorLocation() + FVector(0, 0, 40), Player)
		: 0.5f;
	if (Player->IsFlashlightOn())
	{
		Illumination = FMath::Max(Illumination, 0.5f); // a carried beam paints its carrier
	}
	const float CloseFloor = FMath::Clamp(1.0f - Dist / 450.0f, 0.0f, 1.0f) * 0.7f;
	const float LightFactor = FMath::Max(Illumination, CloseFloor);

	const float DistFactor = FMath::Clamp(1.0f - Dist / SightRange, 0.15f, 1.0f);
	return (VisiblePoints / 3.0f) * LightFactor * DistFactor;
}

void AFCWatcher::SenseTick()
{
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	if (Player == nullptr || Player->GetHealthState() == EFCHealthState::Dead)
	{
		return;
	}

	const float Visibility = ComputePlayerVisibility(Player);
	// Accumulate toward detection; decay when unseen.
	if (Visibility > 0.01f)
	{
		// Fully lit in direct view ~= 3.3 s to Hunt (tuned via FCM4Smoke).
		DetectionMeter = FMath::Min(DetectionMeter + Visibility * 3.0f * 0.1f, 1.0f);
		LastKnownPlayerPos = Player->GetActorLocation();
		bHasLastKnownPlayerPos = true;
	}
	else
	{
		DetectionMeter = FMath::Max(DetectionMeter - 0.12f * 0.1f, 0.0f);
	}

	// Beam sense: a visible flashlight beam is traced toward its origin.
	if (State != EFCWatcherState::Hunt && Visibility < 0.3f)
	{
		if (const UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
		{
			for (const UFCLightRegistry::FBeam& Beam : Registry->GetActiveBeams())
			{
				const FVector BeamMid = Beam.Origin + Beam.Direction * FMath::Min(Beam.Length * 0.5f, 900.0f);
				if (FVector::Dist(GetActorLocation(), BeamMid) < 2200.0f)
				{
					FCollisionQueryParams Params(SCENE_QUERY_STAT(FCWatcherBeam), true, this);
					Params.AddIgnoredActor(Beam.Owner.Get());
					FHitResult Hit;
					if (!GetWorld()->LineTraceSingleByChannel(Hit, Head->GetComponentLocation(), BeamMid, ECC_Visibility, Params))
					{
						InterestPoint = Beam.Origin;
						if (State != EFCWatcherState::Investigate)
						{
							SetState(EFCWatcherState::Investigate, TEXT("saw a beam"));
						}
						break;
					}
				}
			}
		}
	}

	// Light-delta sense: something changed the lights nearby very recently.
	if (State == EFCWatcherState::Patrol || State == EFCWatcherState::Idle)
	{
		if (const UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
		{
			if (Registry->TimeSinceNearbyLightChange(GetActorLocation(), 2000.0f) < 3.0f)
			{
				SetState(EFCWatcherState::Suspicious, TEXT("light delta"));
			}
		}
	}

	// Meter-driven transitions.
	if (DetectionMeter >= 1.0f && State != EFCWatcherState::Hunt)
	{
		SetState(EFCWatcherState::Hunt, TEXT("detection full"));
	}
	else if (DetectionMeter > 0.35f
		&& (State == EFCWatcherState::Patrol || State == EFCWatcherState::Idle))
	{
		InterestPoint = LastKnownPlayerPos;
		SetState(EFCWatcherState::Suspicious, TEXT("meter rising"));
	}

	if (State == EFCWatcherState::Hunt && Visibility <= 0.01f && DetectionMeter < 0.4f)
	{
		InterestPoint = LastKnownPlayerPos;
		SetState(EFCWatcherState::Search, TEXT("lost line of sight"));
	}

	// Contact: the strike (two-strike model on the player side).
	if (FVector::Dist2D(GetActorLocation(), Player->GetActorLocation()) < ContactRange
		&& FMath::Abs(GetActorLocation().Z - Player->GetActorLocation().Z) < 180.0f)
	{
		Player->ApplyHunterContact(TEXT("It saw you in the light."));
		DetectionMeter = 0.4f; // grab resolved; brief reprieve (rest guarantee seed)
		if (State == EFCWatcherState::Hunt)
		{
			SetState(EFCWatcherState::Search, TEXT("contact made"));
		}
	}
}

void AFCWatcher::MoveTowards(const FVector& Target, const float DeltaSeconds)
{
	const FVector To = (Target - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		AddMovementInput(To, 1.0f);
		const FRotator Desired = To.Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), FRotator(0, Desired.Yaw, 0), DeltaSeconds, 3.5f));
	}
}

void AFCWatcher::UpdatePresentation(const float DeltaSeconds)
{
	// The entire animation budget: a slow vertical bob and a head that turns
	// toward what it is thinking about (ADR-0003).
	BobPhase += DeltaSeconds * 2.0f * PI * 0.35f;
	Body->SetRelativeLocation(FVector(0, 0, -8.0f + FMath::Sin(BobPhase) * 6.0f));

	const FVector HeadTo = (InterestPoint - Head->GetComponentLocation()).GetSafeNormal();
	if (!HeadTo.IsNearlyZero())
	{
		const float LocalYaw = FMath::ClampAngle(
			HeadTo.Rotation().Yaw - GetActorRotation().Yaw, -70.0f, 70.0f);
		Head->SetRelativeRotation(FMath::RInterpTo(Head->GetRelativeRotation(),
			FRotator(0, LocalYaw, 0), DeltaSeconds, 4.0f));
	}

	// Awareness broadcast: dim blue / amber / red. Unambiguous (P6).
	FLinearColor Color(0.3f, 0.4f, 1.0f);
	float Intensity = 2.0f;
	switch (State)
	{
	case EFCWatcherState::Suspicious:
	case EFCWatcherState::Investigate:
		Color = FLinearColor(1.0f, 0.62f, 0.15f);
		Intensity = 8.0f;
		break;
	case EFCWatcherState::Hunt:
	case EFCWatcherState::Search:
		Color = FLinearColor(1.0f, 0.12f, 0.08f);
		Intensity = State == EFCWatcherState::Hunt ? 22.0f : 12.0f;
		break;
	default:
		break;
	}
	EyeLight->SetLightColor(Color);
	EyeLight->SetIntensity(Intensity);
}

void AFCWatcher::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	StateTimer += DeltaSeconds;

	// Senses at 10 Hz - cheap, deterministic cadence.
	SenseAccumulator += DeltaSeconds;
	if (SenseAccumulator >= 0.1f)
	{
		SenseAccumulator = 0.0f;
		SenseTick();
	}

	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	const AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;

	switch (State)
	{
	case EFCWatcherState::Patrol:
		if (PatrolPoints.Num() > 0)
		{
			const FVector Target = PatrolPoints[PatrolIndex % PatrolPoints.Num()];
			InterestPoint = Target;
			MoveTowards(Target, DeltaSeconds);
			if (FVector::Dist2D(GetActorLocation(), Target) < 120.0f)
			{
				++PatrolIndex;
			}
		}
		break;

	case EFCWatcherState::Suspicious:
		// Face it; drift toward it slowly; commit or relax.
		MoveTowards(InterestPoint, DeltaSeconds * 0.4f);
		if (StateTimer > 2.5f)
		{
			SetState(DetectionMeter > 0.2f ? EFCWatcherState::Investigate : EFCWatcherState::Patrol,
				DetectionMeter > 0.2f ? TEXT("committing") : TEXT("relaxed"));
		}
		break;

	case EFCWatcherState::Investigate:
		MoveTowards(InterestPoint, DeltaSeconds);
		if (FVector::Dist2D(GetActorLocation(), InterestPoint) < 140.0f || StateTimer > 14.0f)
		{
			SetState(EFCWatcherState::Search, TEXT("reached interest point"));
		}
		break;

	case EFCWatcherState::Hunt:
		if (Player != nullptr)
		{
			InterestPoint = Player->GetActorLocation();
			MoveTowards(InterestPoint, DeltaSeconds);
		}
		break;

	case EFCWatcherState::Search:
		// Work outward from what it last UNDERSTOOD - the LKP if it ever saw
		// the player, otherwise the stimulus it was chasing (never a bogus
		// default: gliding to the world origin was a real caught bug).
		MoveTowards(bHasLastKnownPlayerPos ? LastKnownPlayerPos : InterestPoint, DeltaSeconds);
		if (StateTimer > 8.0f)
		{
			SetState(EFCWatcherState::Patrol, TEXT("search expired"));
		}
		break;

	default:
		break;
	}

	UpdatePresentation(DeltaSeconds);
}
