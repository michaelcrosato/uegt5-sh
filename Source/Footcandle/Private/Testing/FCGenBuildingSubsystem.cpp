#include "Testing/FCGenBuildingSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
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
#include "AI/FCWatcher.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Objectives/FCExtractZone.h"
#include "Objectives/FCKeyItem.h"
#include "Perception/FCLightRegistry.h"
#include "Testing/FCTestStationSubsystem.h"
#include "World/FCDoor.h"
#include "World/FCHideSpot.h"
#include "World/FCNoiseProp.h"

using namespace FC::Gen;

namespace
{
	const TCHAR* GenCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* GenCylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
}

void UFCGenBuildingSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	uint64 Seed = 0;
	FString SeedString;
	if (FParse::Value(FCommandLine::Get(), TEXT("fcgenbuilding="), SeedString))
	{
		Seed = FCString::Strtoui64(*SeedString, nullptr, 10);
		SpawnFromSeed(Seed);
	}
}

AStaticMeshActor* UFCGenBuildingSubsystem::SpawnBox(const FVector& MinCorner, const FVector& MaxCorner)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, GenCubePath);
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

void UFCGenBuildingSubsystem::EmitWallRun(const bool bAlongX, const float FixedPos,
	const float RunStart, const float RunEnd, const float Z0, const float Z1, const int32 Floor)
{
	// Collect portals sitting on this wall run, turn them into openings,
	// emit solid segments + sills + lintels (the Address slicer, generalized).
	struct FOpen { float Start, End, BottomZ, TopZ; };
	TArray<FOpen> Opens;
	for (const FFCGenPortal& Portal : Building.Portals)
	{
		if (Portal.bAlongX != bAlongX)
		{
			continue;
		}
		const float PortalFixed = bAlongX ? Portal.Position.Y : Portal.Position.X;
		const float PortalAlong = bAlongX ? Portal.Position.X : Portal.Position.Y;
		if (!FMath::IsNearlyEqual(PortalFixed, FixedPos, 1.0f)
			|| PortalAlong < RunStart || PortalAlong > RunEnd)
		{
			continue;
		}
		const float FloorZ = Floor * FloorHeight;
		if (Portal.Position.Z < FloorZ || Portal.Position.Z >= FloorZ + FloorHeight)
		{
			continue;
		}
		switch (Portal.Kind)
		{
		case EGenPortalKind::ExteriorDoor:
		case EGenPortalKind::InteriorDoor:
			Opens.Add({ PortalAlong - 50.0f, PortalAlong + 50.0f, FloorZ, FloorZ + 210.0f });
			break;
		case EGenPortalKind::Window:
			Opens.Add({ PortalAlong - 75.0f, PortalAlong + 75.0f, FloorZ + 90.0f, FloorZ + 220.0f });
			break;
		default:
			break;
		}
	}
	Opens.Sort([](const FOpen& A, const FOpen& B) { return A.Start < B.Start; });

	auto Emit = [&](const float A0, const float A1, const float BZ, const float TZ)
	{
		if (A1 - A0 < 1.0f || TZ - BZ < 1.0f)
		{
			return;
		}
		if (bAlongX)
		{
			SpawnBox(FVector(A0, FixedPos - WallThickness * 0.5f, BZ),
				FVector(A1, FixedPos + WallThickness * 0.5f, TZ));
		}
		else
		{
			SpawnBox(FVector(FixedPos - WallThickness * 0.5f, A0, BZ),
				FVector(FixedPos + WallThickness * 0.5f, A1, TZ));
		}
	};

	const float Z1Clamped = Z1;
	float Cursor = RunStart;
	for (const FOpen& Open : Opens)
	{
		Emit(Cursor, Open.Start, Z0, Z1Clamped);
		Emit(Open.Start, Open.End, Z0, Open.BottomZ);
		Emit(Open.Start, Open.End, Open.TopZ, Z1Clamped);
		Cursor = Open.End;
	}
	Emit(Cursor, RunEnd, Z0, Z1Clamped);
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
		// Invalid layouts never enter play (ROADMAP 5.6) - and never silently.
		UE_LOG(LogFootcandle, Error, TEXT("[FCGEN] seed %llu INVALID: %s"),
			Seed, *FString::Join(Problems, TEXT("; ")));
		return false;
	}
	bSpawned = true;
	UWorld* World = GetWorld();
	const float W = Building.FootprintCells.X * CellSize;
	const float D = Building.FootprintCells.Y * CellSize;
	const FIntPoint StairCell(Building.FootprintCells.X - 1, Building.FootprintCells.Y - 1);

	UE_LOG(LogFootcandle, Display,
		TEXT("[FCGEN] seed %llu: %dx%d cells, %d floors, %d rooms, %d portals, %d props"),
		Seed, Building.FootprintCells.X, Building.FootprintCells.Y, Building.Floors,
		Building.Rooms.Num(), Building.Portals.Num(), Building.Props.Num());

	// --- Environment: street, moon, exposure, one sodium streetlight ---
	SpawnBox(FVector(-3000, -3000, -20), FVector(W + 3000, D + 3000, 0));
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

	// --- Slabs (stair holes above ground floor) + roof ---
	for (int32 Floor = 0; Floor <= Building.Floors; ++Floor)
	{
		const float Z = Floor * FloorHeight - (Floor == 0 ? 20.0f : 0.0f);
		const float Z1 = Floor == 0 ? 0.0f : Floor * FloorHeight + 20.0f;
		const bool bHole = Floor > 0 && Floor < Building.Floors + 1 && Building.Floors > 1
			&& Floor <= Building.Floors - 0; // hole wherever stairs pass
		if (bHole && Floor < Building.Floors)
		{
			// Slab minus the stair cell (two L-pieces).
			const float HX0 = StairCell.X * CellSize;
			const float HY0 = StairCell.Y * CellSize;
			if (HY0 > 0)
			{
				SpawnBox(FVector(0, 0, Z), FVector(W, HY0, Z1));
			}
			if (HX0 > 0)
			{
				SpawnBox(FVector(0, HY0, Z), FVector(HX0, D, Z1));
			}
		}
		else
		{
			SpawnBox(FVector(0, 0, Z), FVector(W, D, Z1));
		}
	}

	// --- Walls: interior boundaries + perimeter, per floor ---
	for (int32 Floor = 0; Floor < Building.Floors; ++Floor)
	{
		const float Z0 = Floor * FloorHeight + (Floor == 0 ? 0.0f : 20.0f);
		const float Z1 = (Floor + 1) * FloorHeight;

		// Cell -> room lookup for this floor.
		TArray<int32> CellRoom;
		CellRoom.Init(INDEX_NONE, Building.FootprintCells.X * Building.FootprintCells.Y);
		for (const FFCGenRoom& Room : Building.Rooms)
		{
			if (Room.Floor != Floor)
			{
				continue;
			}
			for (int32 Y = Room.CellMin.Y; Y < Room.CellMax.Y; ++Y)
			{
				for (int32 X = Room.CellMin.X; X < Room.CellMax.X; ++X)
				{
					CellRoom[Y * Building.FootprintCells.X + X] = Room.Id;
				}
			}
		}
		auto RoomAt = [&](const int32 X, const int32 Y) -> int32
		{
			if (X < 0 || Y < 0 || X >= Building.FootprintCells.X || Y >= Building.FootprintCells.Y)
			{
				return INDEX_NONE;
			}
			return CellRoom[Y * Building.FootprintCells.X + X];
		};

		// Vertical wall lines (walls along Y, at x = 0..W).
		for (int32 X = 0; X <= Building.FootprintCells.X; ++X)
		{
			int32 RunStartCell = -1;
			for (int32 Y = 0; Y <= Building.FootprintCells.Y; ++Y)
			{
				const bool bSeparates = Y < Building.FootprintCells.Y
					&& RoomAt(X - 1, Y) != RoomAt(X, Y);
				if (bSeparates && RunStartCell < 0)
				{
					RunStartCell = Y;
				}
				else if (!bSeparates && RunStartCell >= 0)
				{
					EmitWallRun(false, X * CellSize, RunStartCell * CellSize, Y * CellSize, Z0, Z1, Floor);
					RunStartCell = -1;
				}
			}
		}
		// Horizontal wall lines (walls along X, at y = 0..D).
		for (int32 Y = 0; Y <= Building.FootprintCells.Y; ++Y)
		{
			int32 RunStartCell = -1;
			for (int32 X = 0; X <= Building.FootprintCells.X; ++X)
			{
				const bool bSeparates = X < Building.FootprintCells.X
					&& RoomAt(X, Y - 1) != RoomAt(X, Y);
				if (bSeparates && RunStartCell < 0)
				{
					RunStartCell = X;
				}
				else if (!bSeparates && RunStartCell >= 0)
				{
					EmitWallRun(true, Y * CellSize, RunStartCell * CellSize, X * CellSize, Z0, Z1, Floor);
					RunStartCell = -1;
				}
			}
		}

	}
	// Stairs between each floor pair.
	for (int32 Floor = 0; Floor + 1 < Building.Floors; ++Floor)
	{
		const float BaseZ = Floor * FloorHeight + (Floor == 0 ? 0.0f : 20.0f);
		const float SX0 = StairCell.X * CellSize + 10.0f;
		const float SY0 = StairCell.Y * CellSize + 40.0f;
		const float SY1 = (StairCell.Y + 1) * CellSize - 40.0f;
		for (int32 Step = 0; Step < 10; ++Step)
		{
			const float X0 = SX0 + Step * 38.0f;
			SpawnBox(FVector(X0, SY0, BaseZ),
				FVector(X0 + 38.0f, SY1, BaseZ + 32.0f * (Step + 1)));
		}
	}

	// --- Room graph -> the noise model; doors bound to portals ---
	TArray<int32> GraphPortalByGenPortal;
	if (UFCNoiseSubsystem* Noise = World->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->GetRoomGraph() = BuildRoomGraph(Building, FVector::ZeroVector, GraphPortalByGenPortal);
	}
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (const FFCGenPortal& Portal : Building.Portals)
		{
			if (Portal.Kind != EGenPortalKind::ExteriorDoor && Portal.Kind != EGenPortalKind::InteriorDoor)
			{
				continue;
			}
			// Hinge at the -along side of the opening; leaf swings inward.
			const FVector Hinge = Portal.bAlongX
				? Portal.Position + FVector(-50.0f, 0, -105.0f)
				: Portal.Position + FVector(0, -50.0f, -105.0f);
			const FRotator Facing = Portal.bAlongX ? FRotator::ZeroRotator : FRotator(0, 90, 0);
			if (AFCDoor* Door = World->SpawnActor<AFCDoor>(Hinge, Facing, Params))
			{
				if (GraphPortalByGenPortal.IsValidIndex(Portal.Id))
				{
					Door->BindAcousticPortal(GraphPortalByGenPortal[Portal.Id],
						Portal.Kind == EGenPortalKind::ExteriorDoor);
				}
				Doors.Add(Door);
			}
		}
	}

	// --- Lights (registered) ---
	UFCLightRegistry* Registry = World->GetSubsystem<UFCLightRegistry>();
	for (const FFCGenLight& Light : Building.Lights)
	{
		APointLight* Point = World->SpawnActor<APointLight>(Light.Position, FRotator::ZeroRotator);
		if (Point != nullptr)
		{
			Point->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			UPointLightComponent* Component = CastChecked<UPointLightComponent>(Point->GetLightComponent());
			Component->SetIntensityUnits(ELightUnits::Candelas);
			Component->SetIntensity(50.0f);
			Component->SetLightColor(FLinearColor(1.0f, 0.87f, 0.72f));
			Component->SetAttenuationRadius(1200.0f);
			if (Registry != nullptr)
			{
				Registry->RegisterLight(Component);
			}
		}
	}

	// --- Props + hideables ---
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		for (const FFCGenProp& Prop : Building.Props)
		{
			if (Prop.Class == EPropClass::Hideable)
			{
				World->SpawnActor<AFCHideSpot>(Prop.Position, FRotator(0, 45, 0), Params);
			}
			else if (AFCNoiseProp* Spawned = World->SpawnActor<AFCNoiseProp>(Prop.Position, FRotator::ZeroRotator, Params))
			{
				if (Prop.Class == EPropClass::NoiseSmall)
				{
					Spawned->ConfigureMesh(GenCylinderPath, FVector(0.10f, 0.10f, 0.30f), 55.0f);
				}
				else
				{
					Spawned->ConfigureMesh(GenCubePath, FVector(0.30f), 45.0f);
				}
			}
		}
	}

	// --- Stations: exterior + one per floor ---
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

	// --- The M6 slice loop (-fcslice): key upstairs, extraction pad on the
	// street. Hand-wired versions; M8's systemic conditions replace them. ---
	if (FParse::Param(FCommandLine::Get(), TEXT("fcslice")))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Key: center of the largest top-floor room.
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

	// Player on the street facing the entry door.
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
