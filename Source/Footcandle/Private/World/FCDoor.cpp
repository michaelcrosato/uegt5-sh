#include "World/FCDoor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FCTuningSettings.h"
#include "Noise/FCNoiseSubsystem.h"
#include "UObject/ConstructorHelpers.h"

AFCDoor::AFCDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Hinge sits at the actor origin; the leaf hangs +X from it, so the
	// actor is placed at the hinge-side jamb of a 100 cm opening.
	HingePivot = CreateDefaultSubobject<USceneComponent>(TEXT("HingePivot"));
	HingePivot->SetupAttachment(Root);

	Leaf = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Leaf"));
	Leaf->SetupAttachment(HingePivot);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Leaf->SetStaticMesh(CubeMesh.Object);
	}
	// 100 x 6 x 210 leaf, hinge on its -X edge, standing on the floor.
	Leaf->SetRelativeScale3D(FVector(1.0f, 0.06f, 2.1f));
	Leaf->SetRelativeLocation(FVector(50.0f, 0.0f, 105.0f));
	Leaf->SetMobility(EComponentMobility::Movable);
	Leaf->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AFCDoor::Interact(AFCPlayerCharacter* /*User*/, const bool bQuiet)
{
	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	const bool bOpening = TargetAngle < 5.0f;
	TargetAngle = bOpening ? 105.0f : 0.0f;
	SwingSpeed = bQuiet ? QuietSwingSpeed : NormalSwingSpeed;
	bLastSwingWasFast = !bQuiet;
	EmitDoorNoise(bQuiet ? Tuning->NoiseDoorSlow : Tuning->NoiseDoorFast);
}

FString AFCDoor::GetInteractionVerb() const
{
	return IsOpen() ? TEXT("Close") : TEXT("Open");
}

void AFCDoor::EmitDoorNoise(const float Loudness) const
{
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->EmitNoise(GetActorLocation(), Loudness, TEXT("Noise.Source.Door"),
			const_cast<AFCDoor*>(this));
	}
}

void AFCDoor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FMath::IsNearlyEqual(CurrentAngle, TargetAngle, 0.1f))
	{
		return;
	}

	CurrentAngle = FMath::FInterpConstantTo(CurrentAngle, TargetAngle, DeltaSeconds, SwingSpeed);
	HingePivot->SetRelativeRotation(FRotator(0.0f, CurrentAngle, 0.0f));

	// A fast swing that reaches the frame slams.
	if (FMath::IsNearlyEqual(CurrentAngle, TargetAngle, 0.1f)
		&& TargetAngle < 5.0f && bLastSwingWasFast)
	{
		EmitDoorNoise(UFCTuningSettings::Get()->NoiseDoorSlam);
	}
}
