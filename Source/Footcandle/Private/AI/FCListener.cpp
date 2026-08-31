#include "AI/FCListener.h"

#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "GameFramework/PlayerController.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Player/FCPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

AFCListener::AFCListener()
{
	PrimaryActorTick.bCanEverTick = true;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(42.0f, 110.0f);
	Capsule->SetCollisionProfileName(TEXT("Pawn"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// Hunched, narrow - a figure that stops mid-stride and stays stopped.
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	if (CylinderMesh.Succeeded())
	{
		Body->SetStaticMesh(CylinderMesh.Object);
	}
	Body->SetRelativeScale3D(FVector(0.5f, 0.5f, 2.0f));
	Body->SetRelativeRotation(FRotator(8.0f, 0, 0)); // the hunch
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The dish: a wide flat head - all ear.
	Dish = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Dish"));
	Dish->SetupAttachment(Capsule);
	if (CubeMesh.Succeeded())
	{
		Dish->SetStaticMesh(CubeMesh.Object);
	}
	Dish->SetRelativeScale3D(FVector(0.16f, 0.72f, 0.5f));
	Dish->SetRelativeLocation(FVector(6.0f, 0, 108.0f));
	Dish->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Same awareness language as the Watcher (P6): dim blue / amber / red.
	EyeLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EyeLight"));
	EyeLight->SetupAttachment(Dish);
	EyeLight->SetRelativeLocation(FVector(60.0f, 0, 0));
	EyeLight->SetIntensityUnits(ELightUnits::Candelas);
	EyeLight->SetIntensity(2.0f);
	EyeLight->SetLightColor(FLinearColor(0.3f, 0.4f, 1.0f));
	EyeLight->SetAttenuationRadius(600.0f);
	EyeLight->SetMobility(EComponentMobility::Movable);

	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Movement->MaxSpeed = SlideSpeed;
	Movement->Acceleration = 1200.0f;
	Movement->Deceleration = 4000.0f; // it STOPS, hard

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void AFCListener::BeginPlay()
{
	Super::BeginPlay();
	Movement->MaxSpeed = SlideSpeed;
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		NoiseHandle = Noise->OnNoiseEmitted.AddUObject(this, &AFCListener::OnNoise);
	}
}

bool AFCListener::CanMoveNow() const
{
	const UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>();
	const float Floor = Noise != nullptr ? Noise->GetAmbientNoiseFloor() : 0.0f;
	return Floor >= FloorMobilityThreshold
		|| GetWorld()->GetTimeSeconds() - LastWorldNoiseTime <= MoveWindowSeconds;
}

void AFCListener::OnNoise(const FFCNoiseEvent& Event)
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
	if (Perceived <= 0.0f)
	{
		return; // under the floor: never heard (rain is YOUR cover too)
	}
	// Any audible world noise opens its movement window.
	LastWorldNoiseTime = GetWorld()->GetTimeSeconds();

	if (Perceived >= HearingThreshold)
	{
		// It hears precisely - small error even for faint sounds.
		const float Margin = FMath::Clamp(Perceived / 50.0f, 0.0f, 1.0f);
		const float ErrorRadius = FMath::Lerp(120.0f, 20.0f, Margin);
		const float Angle = FMath::Frac(Event.Timestamp * 5.13f) * 2.0f * PI;
		InterestPoint = Event.Origin + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * ErrorRadius;
		bHasInterest = true;
		UE_LOG(LogFootcandle, Verbose, TEXT("[FCLISTENER] heard %s at %.1f"),
			*Event.SourceTag.ToString(), Perceived);
	}
}

void AFCListener::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bMobile = CanMoveNow();

	// Awareness broadcast: frozen = dim blue; interested = amber; closing = red.
	if (!bMobile)
	{
		EyeLight->SetLightColor(FLinearColor(0.3f, 0.4f, 1.0f));
		EyeLight->SetIntensity(2.0f);
		return; // the freeze IS the behavior
	}

	FVector Target;
	if (bHasInterest)
	{
		Target = InterestPoint;
		EyeLight->SetLightColor(FLinearColor(1.0f, 0.2f, 0.1f));
		EyeLight->SetIntensity(14.0f);
		if (FVector::Dist2D(GetActorLocation(), InterestPoint) < 100.0f)
		{
			bHasInterest = false; // arrived; wait for the next sound
		}
	}
	else if (PatrolPoints.Num() > 0)
	{
		Target = PatrolPoints[PatrolIndex % PatrolPoints.Num()];
		EyeLight->SetLightColor(FLinearColor(1.0f, 0.62f, 0.15f));
		EyeLight->SetIntensity(6.0f);
		if (FVector::Dist2D(GetActorLocation(), Target) < 120.0f)
		{
			++PatrolIndex;
		}
	}
	else
	{
		return;
	}

	const FVector To = (Target - GetActorLocation()).GetSafeNormal2D();
	if (!To.IsNearlyZero())
	{
		AddMovementInput(To, 1.0f);
		SetActorRotation(FMath::RInterpTo(GetActorRotation(),
			FRotator(0, To.Rotation().Yaw, 0), DeltaSeconds, 5.0f));
	}

	// Contact: it heard you all the way in.
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	if (Player != nullptr
		&& FVector::Dist2D(GetActorLocation(), Player->GetActorLocation()) < ContactRange
		&& FMath::Abs(GetActorLocation().Z - Player->GetActorLocation().Z) < 180.0f)
	{
		Player->ApplyHunterContact(TEXT("It heard your steps."));
	}
}
