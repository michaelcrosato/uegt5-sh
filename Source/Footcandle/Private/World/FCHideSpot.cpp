#include "World/FCHideSpot.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Player/FCPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

AFCHideSpot::AFCHideSpot()
{
	PrimaryActorTick.bCanEverTick = false;

	// A locker shell: an open-fronted box the player steps into. Built from
	// engine primitives at M1; kit mesh later. The shell is 3 walls + top so
	// the interior is genuinely dark - the lighting IS the concealment.
	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	SetRootComponent(Shell);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Shell->SetStaticMesh(CubeMesh.Object);
	}
	// Back panel of the locker; sides added by the scene builder.
	Shell->SetRelativeScale3D(FVector(0.9f, 0.1f, 2.0f));
	Shell->SetMobility(EComponentMobility::Movable);

	HidePoint = CreateDefaultSubobject<USceneComponent>(TEXT("HidePoint"));
	HidePoint->SetupAttachment(Shell);
	HidePoint->SetRelativeLocation(FVector(0.0f, 400.0f, 0.0f)); // in front of back panel (unscaled)
}

void AFCHideSpot::Interact(AFCPlayerCharacter* User, const bool /*bQuiet*/)
{
	if (User == nullptr)
	{
		return;
	}

	UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>();

	if (!Occupant.IsValid())
	{
		// Enter: camera cut, no animation. Store the exit spot first.
		Occupant = User;
		ExitLocation = User->GetActorLocation();
		User->SetActorLocation(HidePoint->GetComponentLocation()
			+ FVector(0, 0, User->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		User->GetCharacterMovement()->DisableMovement();
		if (Noise != nullptr)
		{
			Noise->EmitNoise(GetActorLocation(), 5.0f, TEXT("Noise.Source.HideEntry"), User);
		}
	}
	else if (Occupant.Get() == User)
	{
		// Exit: restore, clearance-checked; nudge upward if something now
		// blocks the stored spot (never trap the player).
		FVector Exit = ExitLocation;
		FCollisionShape Capsule = FCollisionShape::MakeCapsule(
			User->GetCapsuleComponent()->GetScaledCapsuleRadius(),
			User->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		FCollisionQueryParams Params(SCENE_QUERY_STAT(FCHideExit), false, User);
		if (GetWorld()->OverlapBlockingTestByChannel(Exit, FQuat::Identity, ECC_Pawn, Capsule, Params))
		{
			Exit += FVector(0, 0, 50.0f);
		}
		User->SetActorLocation(Exit);
		User->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Occupant = nullptr;
	}
}

FString AFCHideSpot::GetInteractionVerb() const
{
	return IsOccupied() ? TEXT("Leave") : TEXT("Hide");
}
