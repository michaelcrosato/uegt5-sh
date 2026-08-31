#include "World/FCNoiseProp.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Noise/FCNoiseSubsystem.h"

AFCNoiseProp::AFCNoiseProp()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	// Physics props stay out of the RT scene cheaply later (ROADMAP 6.5);
	// for now they are small and few.
}

void AFCNoiseProp::ConfigureMesh(const TCHAR* MeshPath, const FVector& Scale, const float InBaseLoudness)
{
	if (UStaticMesh* LoadedMesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
	{
		Mesh->SetStaticMesh(LoadedMesh);
	}
	SetActorScale3D(Scale);
	BaseLoudness = InBaseLoudness;
	Mesh->SetMassOverrideInKg(NAME_None, 1.5f * Scale.X, true);
}

void AFCNoiseProp::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other,
	UPrimitiveComponent* OtherComp, const bool bSelfMoved, const FVector HitLocation,
	const FVector HitNormal, const FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastNoiseTime < 0.5f)
	{
		return; // AUD-05: no runaway noise loops from settling contacts
	}

	const float ImpulseSize = NormalImpulse.Size();
	if (ImpulseSize < 60.0f)
	{
		return; // resting/grazing contact
	}

	LastNoiseTime = Now;
	const float Loudness = FMath::Clamp(
		BaseLoudness * FMath::Min(ImpulseSize / 600.0f, 1.6f), 10.0f, 95.0f);
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->EmitNoise(HitLocation, Loudness, TEXT("Noise.Source.Impact"), this);
	}
}
