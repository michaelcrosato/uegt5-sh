#include "Testing/FCGenBuildingSubsystem.h"

#include "AI/FCWatcher.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SpotLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Objectives/FCExtractZone.h"
#include "Objectives/FCKeyItem.h"
#include "Perception/FCLightRegistry.h"
#include "Testing/FCTestStationSubsystem.h"
#include "World/FCBuildingSpawner.h"
#include "World/FCDoor.h"

using namespace FC::Gen;

void UFCGenBuildingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	FString SeedString;
	if (FParse::Value(FCommandLine::Get(), TEXT("fcgenbuilding="), SeedString))
	{
		SpawnFromSeed(FCString::Strtoui64(*SeedString, nullptr, 10));
	}
}

AStaticMeshActor* UFCGenBuildingSubsystem::SpawnBox(const FVector& MinCorner, const FVector& MaxCorner)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube == nullptr)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(
		(MinCorner + MaxCorner) * 0.5f, FRotator::ZeroRotator, Params);
	if (Actor != nullptr)
	{
		Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Actor->SetActorScale3D((MaxCorner - MinCorner) / 100.0f);
	}
	return Actor;
}

void UFCGenBuildingSubsystem::EmitWallRun(bool, float, float, float, float, float, int32)
{
	// Superseded by FC::Spawn (World/FCBuildingSpawner) - kept only so the
	// header stays stable this milestone; remove with the M7 cleanup pass.
}

bool UFCGenBuildingSubsystem::SpawnFromSeed(const uint64 Seed)
{
	if (bSpawned)
	{
		return false;
	}

	Building = GenerateBuilding(Seed, /*BuildingId*/ 1, FFCBuildingRules());
	const TArray<FString> Problems = ValidateBuilding(Building);
	if (Problems.Num() > 0)
	{
		UE_LOG(LogFootcandle, Error, TEXT("[FCGEN] seed %llu INVALID: %s"),
			Seed, *FString::Join(Problems, TEXT("; ")));
		return false;
	}
	bSpawned = true;
	UWorld* World = GetWorld();
	const float W = Building.FootprintCells.X * CellSize;
	const float D = Building.FootprintCells.Y * CellSize;

	UE_LOG(LogFootcandle, Display,
		TEXT("[FCGEN] seed %llu: %dx%d cells, %d floors, %d rooms, %d portals, %d props"),
		Seed, Building.FootprintCells.X, Building.FootprintCells.Y, Building.Floors,
		Building.Rooms.Num(), Building.Portals.Num(), Building.Props.Num());

	// --- Environment ---
	SpawnBox(FVector(-3000, -3000, -20), FVector(W + 3000, D + 3000, 0));
	if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator))
	{
		UExponentialHeightFogComponent* FogComponent = Fog->GetComponent();
		FogComponent->SetMobility(EComponentMobility::Movable);
		FogComponent->SetFogDensity(0.018f);
		FogComponent->SetVolumetricFog(true);
	}
	{
		ADirectionalLight* Moon = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000), FRotator::ZeroRotator);
		if (Moon != nullptr)
		{
			Moon->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			UDirectionalLightComponent* Component = CastChecked<UDirectionalLightComponent>(Moon->GetLightComponent());
			Component->SetWorldRotation(FRotator(-35.0f, 40.0f, 0.0f));
			Component->SetIntensity(0.08f);
			Component->SetLightColor(FLinearColor(0.55f, 0.65f, 0.95f));
			if (UFCLightRegistry* Registry = World->GetSubsystem<UFCLightRegistry>())
			{
				Registry->RegisterLight(Component);
			}
		}
	}
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APostProcessVolume* PP = World->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
		{
			PP->bUnbound = true;
			PP->Settings.bOverride_AutoExposureMinBrightness = true;
			PP->Settings.AutoExposureMinBrightness = 0.02f;
			PP->Settings.bOverride_AutoExposureMaxBrightness = true;
			PP->Settings.AutoExposureMaxBrightness = 0.18f;
			PP->Settings.bOverride_AutoExposureBias = true;
			PP->Settings.AutoExposureBias = -0.4f;
		}
	}
	{
		ASpotLight* Street = World->SpawnActor<ASpotLight>(FVector(W * 0.5f, -650, 550), FRotator::ZeroRotator);
		if (Street != nullptr)
		{
			Street->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			USpotLightComponent* Component = CastChecked<USpotLightComponent>(Street->GetLightComponent());
			Component->SetWorldRotation(FRotator(-55.0f, 90.0f, 0.0f));
			Component->SetIntensityUnits(ELightUnits::Candelas);
			Component->SetIntensity(800.0f);
			Component->SetLightColor(FLinearColor(1.0f, 0.64f, 0.23f));
			Component->SetAttenuationRadius(2200.0f);
			Component->SetInnerConeAngle(20.0f);
			Component->SetOuterConeAngle(32.0f);
			Component->SetVolumetricScatteringIntensity(2.5f);
			if (UFCLightRegistry* Registry = World->GetSubsystem<UFCLightRegistry>())
			{
				Registry->RegisterLight(Component);
			}
		}
	}

	// --- Geometry + interior via the shared spawner ---
	if (UFCNoiseSubsystem* Noise = World->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->GetRoomGraph() = BuildRoomGraph(Building, FVector::ZeroVector, Spawned.GraphPortalByGenPortal);
	}
	FC::Spawn::SpawnShell(World, Building, FVector::ZeroVector, Spawned);
	FC::Spawn::SpawnDetail(World, Building, FVector::ZeroVector, Spawned);
	Doors = Spawned.Doors;

	// --- Slice objectives (-fcslice) ---
	if (FParse::Param(FCommandLine::Get(), TEXT("fcslice")))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		int32 KeyRoom = INDEX_NONE;
		int32 BestArea = 0;
		for (const FFCGenRoom& Room : Building.Rooms)
		{
			const int32 Area = (Room.CellMax.X - Room.CellMin.X) * (Room.CellMax.Y - Room.CellMin.Y);
			if (Room.Floor == Building.Floors - 1 && Area > BestArea)
			{
				KeyRoom = Room.Id;
				BestArea = Area;
			}
		}
		if (KeyRoom != INDEX_NONE)
		{
			const FFCGenRoom& Room = Building.Rooms[KeyRoom];
			World->SpawnActor<AFCKeyItem>(FVector(
				(Room.CellMin.X + Room.CellMax.X) * 0.5f * CellSize,
				(Room.CellMin.Y + Room.CellMax.Y) * 0.5f * CellSize,
				Room.Floor * FloorHeight + 60.0f), FRotator::ZeroRotator, Params);
		}
		World->SpawnActor<AFCExtractZone>(FVector(W * 0.5f, -1800, 10), FRotator::ZeroRotator, Params);
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("fcwatcher")))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AFCWatcher* Watcher = World->SpawnActor<AFCWatcher>(
			FVector(W + 1500, -800, 140), FRotator(0, 180, 0), Params))
		{
			Watcher->SetPatrolPoints({
				FVector(W + 1500, -800, 140),
				FVector(-1500, -800, 140),
			});
		}
	}

	// --- Stations + player ---
	if (UFCTestStationSubsystem* Stations = World->GetSubsystem<UFCTestStationSubsystem>())
	{
		Stations->RegisterStation(TEXT("GenExterior"),
			FVector(W * 0.5f, -1100, 300), FRotator(-8.0f, 90.0f, 0.0f));
		for (int32 Floor = 0; Floor < Building.Floors; ++Floor)
		{
			Stations->RegisterStation(*FString::Printf(TEXT("GenFloor%d"), Floor),
				FVector(W * 0.35f, D * 0.4f, Floor * FloorHeight + 165.0f),
				FRotator(-4.0f, 40.0f, 0.0f));
		}
	}
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->TeleportTo(FVector(W * 0.5f, -400, 120), FRotator(0, 90, 0), false, true);
			PC->SetControlRotation(FRotator(0, 90, 0));
		}
	}
	return true;
}
