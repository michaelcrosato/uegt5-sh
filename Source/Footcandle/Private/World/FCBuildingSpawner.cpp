#include "World/FCBuildingSpawner.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Perception/FCLightRegistry.h"
#include "World/FCDoor.h"
#include "World/FCHideSpot.h"
#include "World/FCNoiseProp.h"

using namespace FC::Gen;

namespace
{
	const TCHAR* SpawnCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* SpawnCylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	AStaticMeshActor* SpawnBoxActor(UWorld* World, const FVector& MinCorner, const FVector& MaxCorner)
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, SpawnCubePath);
		if (Cube == nullptr)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
			(MinCorner + MaxCorner) * 0.5f, FRotator::ZeroRotator, Params);
		if (Actor != nullptr)
		{
			Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Actor->SetActorScale3D((MaxCorner - MinCorner) / 100.0f);
		}
		return Actor;
	}

	void EmitWallRun(UWorld* World, const FFCBuildingData& Building, const FVector& Origin,
		const bool bAlongX, const float FixedPos, const float RunStart, const float RunEnd,
		const float Z0, const float Z1, const int32 Floor, TArray<TWeakObjectPtr<AActor>>& OutActors)
	{
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
			const float PortalFloorZ = Floor * FloorHeight;
			if (Portal.Position.Z < PortalFloorZ || Portal.Position.Z >= PortalFloorZ + FloorHeight)
			{
				continue;
			}
			switch (Portal.Kind)
			{
			case EGenPortalKind::ExteriorDoor:
			case EGenPortalKind::InteriorDoor:
				Opens.Add({ PortalAlong - 50.0f, PortalAlong + 50.0f, PortalFloorZ, PortalFloorZ + 210.0f });
				break;
			case EGenPortalKind::Window:
				Opens.Add({ PortalAlong - 75.0f, PortalAlong + 75.0f, PortalFloorZ + 90.0f, PortalFloorZ + 220.0f });
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
			AStaticMeshActor* Actor = bAlongX
				? SpawnBoxActor(World, Origin + FVector(A0, FixedPos - WallThickness * 0.5f, BZ),
					Origin + FVector(A1, FixedPos + WallThickness * 0.5f, TZ))
				: SpawnBoxActor(World, Origin + FVector(FixedPos - WallThickness * 0.5f, A0, BZ),
					Origin + FVector(FixedPos + WallThickness * 0.5f, A1, TZ));
			if (Actor != nullptr)
			{
				OutActors.Add(Actor);
			}
		};

		float Cursor = RunStart;
		for (const FOpen& Open : Opens)
		{
			Emit(Cursor, Open.Start, Z0, Z1);
			Emit(Open.Start, Open.End, Z0, Open.BottomZ);
			Emit(Open.Start, Open.End, Open.TopZ, Z1);
			Cursor = Open.End;
		}
		Emit(Cursor, RunEnd, Z0, Z1);
	}

	// Emit the wall lines of one floor; perimeter and interior split by the
	// bPerimeter flag so shell and detail each take their half.
	void EmitFloorWalls(UWorld* World, const FFCBuildingData& Building, const FVector& Origin,
		const int32 Floor, const bool bPerimeter, TArray<TWeakObjectPtr<AActor>>& OutActors)
	{
		const float Z0 = Floor * FloorHeight + (Floor == 0 ? 0.0f : 20.0f);
		const float Z1 = (Floor + 1) * FloorHeight;

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

		for (int32 X = 0; X <= Building.FootprintCells.X; ++X)
		{
			const bool bIsPerimeter = X == 0 || X == Building.FootprintCells.X;
			if (bIsPerimeter != bPerimeter)
			{
				continue;
			}
			int32 RunStartCell = -1;
			for (int32 Y = 0; Y <= Building.FootprintCells.Y; ++Y)
			{
				const bool bSeparates = Y < Building.FootprintCells.Y && RoomAt(X - 1, Y) != RoomAt(X, Y);
				if (bSeparates && RunStartCell < 0)
				{
					RunStartCell = Y;
				}
				else if (!bSeparates && RunStartCell >= 0)
				{
					EmitWallRun(World, Building, Origin, false, X * CellSize,
						RunStartCell * CellSize, Y * CellSize, Z0, Z1, Floor, OutActors);
					RunStartCell = -1;
				}
			}
		}
		for (int32 Y = 0; Y <= Building.FootprintCells.Y; ++Y)
		{
			const bool bIsPerimeter = Y == 0 || Y == Building.FootprintCells.Y;
			if (bIsPerimeter != bPerimeter)
			{
				continue;
			}
			int32 RunStartCell = -1;
			for (int32 X = 0; X <= Building.FootprintCells.X; ++X)
			{
				const bool bSeparates = X < Building.FootprintCells.X && RoomAt(X, Y - 1) != RoomAt(X, Y);
				if (bSeparates && RunStartCell < 0)
				{
					RunStartCell = X;
				}
				else if (!bSeparates && RunStartCell >= 0)
				{
					EmitWallRun(World, Building, Origin, true, Y * CellSize,
						RunStartCell * CellSize, X * CellSize, Z0, Z1, Floor, OutActors);
					RunStartCell = -1;
				}
			}
		}
	}
}

namespace FC::Spawn
{
	void SpawnShell(UWorld* World, const FFCBuildingData& Building, const FVector& Origin,
		FFCSpawnedBuilding& Out)
	{
		const float W = Building.FootprintCells.X * CellSize;
		const float D = Building.FootprintCells.Y * CellSize;
		const FIntPoint StairCell(Building.FootprintCells.X - 1, Building.FootprintCells.Y - 1);

		// Slabs + roof (stair holes above ground).
		for (int32 Floor = 0; Floor <= Building.Floors; ++Floor)
		{
			const float Z = Floor * FloorHeight - (Floor == 0 ? 20.0f : 0.0f);
			const float Z1 = Floor == 0 ? 0.0f : Floor * FloorHeight + 20.0f;
			if (Floor > 0 && Floor < Building.Floors && Building.Floors > 1)
			{
				const float HX0 = StairCell.X * CellSize;
				const float HY0 = StairCell.Y * CellSize;
				if (HY0 > 0)
				{
					Out.ShellActors.Add(SpawnBoxActor(World, Origin + FVector(0, 0, Z), Origin + FVector(W, HY0, Z1)));
				}
				if (HX0 > 0)
				{
					Out.ShellActors.Add(SpawnBoxActor(World, Origin + FVector(0, HY0, Z), Origin + FVector(HX0, D, Z1)));
				}
			}
			else
			{
				Out.ShellActors.Add(SpawnBoxActor(World, Origin + FVector(0, 0, Z), Origin + FVector(W, D, Z1)));
			}
		}

		// Perimeter walls + stairs.
		for (int32 Floor = 0; Floor < Building.Floors; ++Floor)
		{
			EmitFloorWalls(World, Building, Origin, Floor, /*bPerimeter*/ true, Out.ShellActors);
		}
		for (int32 Floor = 0; Floor + 1 < Building.Floors; ++Floor)
		{
			const float BaseZ = Floor * FloorHeight + (Floor == 0 ? 0.0f : 20.0f);
			const float SX0 = StairCell.X * CellSize + 10.0f;
			const float SY0 = StairCell.Y * CellSize + 40.0f;
			const float SY1 = (StairCell.Y + 1) * CellSize - 40.0f;
			for (int32 Step = 0; Step < 10; ++Step)
			{
				const float X0 = SX0 + Step * 38.0f;
				Out.ShellActors.Add(SpawnBoxActor(World,
					Origin + FVector(X0, SY0, BaseZ),
					Origin + FVector(X0 + 38.0f, SY1, BaseZ + 32.0f * (Step + 1))));
			}
		}
	}

	void SpawnDetail(UWorld* World, const FFCBuildingData& Building, const FVector& Origin,
		FFCSpawnedBuilding& Out)
	{
		if (Out.bHasDetail)
		{
			return;
		}
		Out.bHasDetail = true;

		for (int32 Floor = 0; Floor < Building.Floors; ++Floor)
		{
			EmitFloorWalls(World, Building, Origin, Floor, /*bPerimeter*/ false, Out.DetailActors);
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		// Doors bound to graph portals (caller refreshed the graph first).
		for (const FFCGenPortal& Portal : Building.Portals)
		{
			if (Portal.Kind != EGenPortalKind::ExteriorDoor && Portal.Kind != EGenPortalKind::InteriorDoor)
			{
				continue;
			}
			const FVector Hinge = Origin + (Portal.bAlongX
				? Portal.Position + FVector(-50.0f, 0, -105.0f)
				: Portal.Position + FVector(0, -50.0f, -105.0f));
			const FRotator Facing = Portal.bAlongX ? FRotator::ZeroRotator : FRotator(0, 90, 0);
			if (AFCDoor* Door = World->SpawnActor<AFCDoor>(Hinge, Facing, Params))
			{
				if (Out.GraphPortalByGenPortal.IsValidIndex(Portal.Id))
				{
					Door->BindAcousticPortal(Out.GraphPortalByGenPortal[Portal.Id],
						Portal.Kind == EGenPortalKind::ExteriorDoor);
				}
				Out.Doors.Add(Door);
				Out.DetailActors.Add(Door);
			}
		}

		// Lights.
		UFCLightRegistry* Registry = World->GetSubsystem<UFCLightRegistry>();
		for (const FFCGenLight& Light : Building.Lights)
		{
			APointLight* Point = World->SpawnActor<APointLight>(Origin + Light.Position, FRotator::ZeroRotator);
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
				Out.DetailActors.Add(Point);
			}
		}

		// Props + hideables.
		for (const FFCGenProp& Prop : Building.Props)
		{
			if (Prop.Class == EPropClass::Hideable)
			{
				if (AActor* Hide = World->SpawnActor<AFCHideSpot>(Origin + Prop.Position, FRotator(0, 45, 0), Params))
				{
					Out.DetailActors.Add(Hide);
				}
			}
			else if (AFCNoiseProp* Spawned = World->SpawnActor<AFCNoiseProp>(Origin + Prop.Position, FRotator::ZeroRotator, Params))
			{
				if (Prop.Class == EPropClass::NoiseSmall)
				{
					Spawned->ConfigureMesh(SpawnCylinderPath, FVector(0.10f, 0.10f, 0.30f), 55.0f);
				}
				else
				{
					Spawned->ConfigureMesh(SpawnCubePath, FVector(0.30f), 45.0f);
				}
				Out.DetailActors.Add(Spawned);
			}
		}
	}

	void DespawnDetail(FFCSpawnedBuilding& Spawned)
	{
		for (const TWeakObjectPtr<AActor>& Actor : Spawned.DetailActors)
		{
			if (Actor.IsValid())
			{
				Actor->Destroy();
			}
		}
		Spawned.DetailActors.Empty();
		Spawned.Doors.Empty();
		Spawned.bHasDetail = false;
	}
}
