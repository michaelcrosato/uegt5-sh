#include "Testing/FCDevSceneSubsystem.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Testing/FCTestStationSubsystem.h"

namespace
{
	FAutoConsoleCommandWithWorld GFCDevSceneCmd(
		TEXT("fc.DevScene.Spawn"),
		TEXT("Spawn the code-defined lighting dev scene."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld* World)
			{
				if (World != nullptr)
				{
					if (UFCDevSceneSubsystem* Subsystem = World->GetSubsystem<UFCDevSceneSubsystem>())
					{
						Subsystem->SpawnScene();
					}
				}
			}));

	// The game's palette, by light: sodium amber, cool moon-blue fill, warm interior.
	const FLinearColor SodiumAmber(1.0f, 0.64f, 0.23f);
	const FLinearColor CoolFill(0.35f, 0.5f, 1.0f);
	const FLinearColor WarmWhite(1.0f, 0.9f, 0.8f);
}

void UFCDevSceneSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld())
	{
		return;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("fcdevscene")))
	{
		SpawnScene();
	}
}

AStaticMeshActor* UFCDevSceneSubsystem::SpawnMesh(const TCHAR* MeshPath, const FVector& Location,
	const FRotator& Rotation, const FVector& Scale)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (Mesh == nullptr)
	{
		UE_LOG(LogFootcandle, Warning, TEXT("[FCDEV] Missing mesh %s"), MeshPath);
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Rotation, Params);
	if (Actor == nullptr)
	{
		return nullptr;
	}

	UStaticMeshComponent* Component = Actor->GetStaticMeshComponent();
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	Actor->SetActorScale3D(Scale);
	return Actor;
}

void UFCDevSceneSubsystem::SpawnScene()
{
	if (bSpawned)
	{
		return;
	}
	bSpawned = true;

	const TCHAR* Cube = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* Cylinder = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* Sphere = TEXT("/Engine/BasicShapes/Sphere.Sphere");

	// Geometry: a ground slab, two walls forming a corner, a pillar row for
	// long cast shadows, and a few loose primitives. All dimensions cm.
	SpawnMesh(Cube, FVector(0, 0, -10), FRotator::ZeroRotator, FVector(40, 40, 0.2));   // ground 40x40 m
	SpawnMesh(Cube, FVector(0, 1000, 150), FRotator::ZeroRotator, FVector(30, 0.3, 3)); // back wall
	SpawnMesh(Cube, FVector(-1200, 0, 150), FRotator::ZeroRotator, FVector(0.3, 24, 3)); // side wall
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const float X = -600.0f + Index * 300.0f;
		SpawnMesh(Cylinder, FVector(X, 200, 150), FRotator::ZeroRotator, FVector(0.6, 0.6, 3));
	}
	SpawnMesh(Sphere, FVector(300, -300, 120), FRotator::ZeroRotator, FVector(1.5));
	SpawnMesh(Cube, FVector(-300, -500, 50), FRotator(0, 30, 0), FVector(1));
	SpawnMesh(Cube, FVector(150, -650, 30), FRotator(0, -15, 0), FVector(0.6));

	// Lights - all movable, all shadow-casting. Values in candela.
	{
		ASpotLight* Spot = GetWorld()->SpawnActor<ASpotLight>(
			FVector(900, -700, 350),
			FRotator(-10.0f, 135.0f, 0.0f));
		if (Spot != nullptr)
		{
			Spot->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			USpotLightComponent* Component = CastChecked<USpotLightComponent>(Spot->GetLightComponent());
			Component->SetWorldRotation(FRotator(-10.0f, 135.0f, 0.0f)); // see address scene note
			Component->SetIntensityUnits(ELightUnits::Candelas);
			Component->SetIntensity(2500.0f);
			Component->SetLightColor(SodiumAmber);
			Component->SetAttenuationRadius(5000.0f);
			Component->SetInnerConeAngle(22.0f);
			Component->SetOuterConeAngle(38.0f);
			Component->SetVolumetricScatteringIntensity(4.0f);
		}
	}
	{
		APointLight* Point = GetWorld()->SpawnActor<APointLight>(FVector(-800, 600, 250), FRotator::ZeroRotator);
		if (Point != nullptr)
		{
			Point->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			UPointLightComponent* Component = CastChecked<UPointLightComponent>(Point->GetLightComponent());
			Component->SetIntensityUnits(ELightUnits::Candelas);
			Component->SetIntensity(40.0f);
			Component->SetLightColor(CoolFill);
			Component->SetAttenuationRadius(2500.0f);
		}
	}
	{
		APointLight* Point = GetWorld()->SpawnActor<APointLight>(FVector(0, 0, 700), FRotator::ZeroRotator);
		if (Point != nullptr)
		{
			Point->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			UPointLightComponent* Component = CastChecked<UPointLightComponent>(Point->GetLightComponent());
			Component->SetIntensityUnits(ELightUnits::Candelas);
			Component->SetIntensity(100.0f);
			Component->SetLightColor(WarmWhite);
			Component->SetAttenuationRadius(5000.0f);
		}
	}

	// Thin fog so the amber spot reads as a visible beam.
	if (AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(FVector(0, 0, 0), FRotator::ZeroRotator))
	{
		UExponentialHeightFogComponent* Component = Fog->GetComponent();
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetFogDensity(0.02f);
		Component->SetVolumetricFog(true);
	}

	// Stations for the visual tour (docs/ROADMAP.md M0 exit criteria).
	if (UFCTestStationSubsystem* Stations = GetWorld()->GetSubsystem<UFCTestStationSubsystem>())
	{
		Stations->RegisterStation(TEXT("Overview"), FVector(1400, -1400, 900), FRotator(-28.0f, 135.0f, 0.0f));
		Stations->RegisterStation(TEXT("PillarShadows"), FVector(-900, -300, 160), FRotator(-4.0f, 18.0f, 0.0f));
		Stations->RegisterStation(TEXT("WallBounce"), FVector(0, -200, 180), FRotator(0.0f, 90.0f, 0.0f));
		Stations->RegisterStation(TEXT("DarkCorner"), FVector(-700, -1000, 170), FRotator(2.0f, 160.0f, 0.0f));
	}

	UE_LOG(LogFootcandle, Display, TEXT("[FCDEV] Dev scene spawned: 3 shadow-casting movable lights, volumetric fog, 4 stations"));
}
