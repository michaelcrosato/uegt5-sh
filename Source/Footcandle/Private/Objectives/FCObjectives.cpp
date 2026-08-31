#include "Objectives/FCExtractZone.h"
#include "Objectives/FCKeyItem.h"
#include "Objectives/FCRunSubsystem.h"

#include "AI/FCDirectorSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Perception/FCLightRegistry.h"
#include "Player/FCPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

// ---------- Run state ----------

void UFCRunSubsystem::NotifyConditionSatisfied(const TCHAR* Reason)
{
	++ConditionsSatisfied;
	UE_LOG(LogFootcandle, Display, TEXT("[FCRUN] condition %d/%d satisfied (%s)"),
		ConditionsSatisfied, ConditionsRequired, Reason);
	// Each satisfied condition escalates the run (ROADMAP 8.5: +15 pressure).
	if (UFCDirectorSubsystem* Director = GetWorld()->GetSubsystem<UFCDirectorSubsystem>())
	{
		Director->AddPressure(15.0f, TEXT("condition satisfied"));
	}
}

void UFCRunSubsystem::NotifyExtractionStarted()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCRUN] extraction commit started"));
}

void UFCRunSubsystem::NotifyExtractionComplete()
{
	bWon = true;
	UE_LOG(LogFootcandle, Display,
		TEXT("[FCRUN] ESCAPED - run complete (noise events emitted: %d)"), NoiseEventCount);
}

// ---------- Key ----------

AFCKeyItem::AFCKeyItem()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.05f));
	Mesh->SetMobility(EComponentMobility::Movable);

	// The key announces itself in light - and taking it takes the light.
	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Mesh);
	Glow->SetRelativeLocation(FVector(0, 0, 60.0f));
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(10.0f);
	Glow->SetLightColor(FLinearColor(0.35f, 1.0f, 0.55f)); // exit-sign green
	Glow->SetAttenuationRadius(800.0f);
	Glow->SetMobility(EComponentMobility::Movable);
}

void AFCKeyItem::Interact(AFCPlayerCharacter* User, const bool /*bQuiet*/)
{
	if (UFCRunSubsystem* Run = GetWorld()->GetSubsystem<UFCRunSubsystem>())
	{
		Run->NotifyKeyTaken();
	}
	if (UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
	{
		Registry->NotifyLightStateChanged(GetActorLocation()); // the glow dying is a delta
	}
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->EmitNoise(GetActorLocation(), 10.0f, TEXT("Noise.Source.Pickup"), User);
	}
	Destroy();
}

// ---------- Extraction ----------

AFCExtractZone::AFCExtractZone()
{
	PrimaryActorTick.bCanEverTick = true;

	Beacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beacon"));
	SetRootComponent(Beacon);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Beacon->SetStaticMesh(CubeMesh.Object);
	}
	Beacon->SetRelativeScale3D(FVector(1.6f, 1.6f, 0.15f)); // a pad
	Beacon->SetMobility(EComponentMobility::Movable);

	BeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BeaconLight"));
	BeaconLight->SetupAttachment(Beacon);
	BeaconLight->SetRelativeLocation(FVector(0, 0, 700.0f));
	BeaconLight->SetIntensityUnits(ELightUnits::Candelas);
	BeaconLight->SetIntensity(6.0f); // dim until committing
	BeaconLight->SetLightColor(FLinearColor(1.0f, 0.15f, 0.1f)); // the single red
	BeaconLight->SetAttenuationRadius(2600.0f);
	BeaconLight->SetMobility(EComponentMobility::Movable);
}

bool AFCExtractZone::CanInteract(const AFCPlayerCharacter* User) const
{
	const UFCRunSubsystem* Run = GetWorld()->GetSubsystem<UFCRunSubsystem>();
	return Run != nullptr && Run->HasKey() && !Run->IsWon() && !bCommitting;
}

FString AFCExtractZone::GetInteractionVerb() const
{
	const UFCRunSubsystem* Run = GetWorld()->GetSubsystem<UFCRunSubsystem>();
	if (Run != nullptr && !Run->HasKey())
	{
		return TEXT("Locked");
	}
	return bCommitting ? TEXT("Hold on") : TEXT("Extract");
}

void AFCExtractZone::Interact(AFCPlayerCharacter* User, const bool /*bQuiet*/)
{
	if (!CanInteract(User))
	{
		return;
	}
	bCommitting = true;
	CommitRemaining = CommitSeconds;
	Committer = User;

	// Committing is LOUD and BRIGHT - the ending announces itself to
	// everything that hunts by either channel (ROADMAP 4.1).
	BeaconLight->SetIntensity(60.0f);
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->EmitNoise(GetActorLocation(), 75.0f, TEXT("Noise.Source.Extraction"), User);
	}
	if (UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
	{
		Registry->NotifyLightStateChanged(GetActorLocation());
	}
	if (UFCRunSubsystem* Run = GetWorld()->GetSubsystem<UFCRunSubsystem>())
	{
		Run->NotifyExtractionStarted();
	}
}

void AFCExtractZone::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bCommitting)
	{
		return;
	}

	const AFCPlayerCharacter* Player = Committer.Get();
	if (Player == nullptr || Player->GetHealthState() == EFCHealthState::Dead)
	{
		bCommitting = false;
		BeaconLight->SetIntensity(6.0f);
		return;
	}
	// The player must HOLD the zone - leaving pauses the clock.
	if (FVector::Dist2D(Player->GetActorLocation(), GetActorLocation()) > 350.0f)
	{
		return;
	}

	CommitRemaining -= DeltaSeconds;
	// The beacon pulses harder as the window closes.
	BeaconLight->SetIntensity(60.0f + 40.0f * FMath::Sin(CommitRemaining * 6.0f));
	if (CommitRemaining <= 0.0f)
	{
		bCommitting = false;
		BeaconLight->SetLightColor(FLinearColor(0.35f, 1.0f, 0.55f));
		BeaconLight->SetIntensity(80.0f);
		if (UFCRunSubsystem* Run = GetWorld()->GetSubsystem<UFCRunSubsystem>())
		{
			Run->NotifyExtractionComplete();
		}
	}
}
