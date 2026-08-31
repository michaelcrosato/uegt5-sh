#include "FCBuildingGen.h"

#include "FCGenSeed.h"
#include "FCGenStageIds.h"

namespace FC::Gen
{
	namespace
	{
		// Recursive cell-rect split into rooms. Deterministic: all randomness
		// from the passed stream, arrays only (AGENTS.md rule 3).
		void SplitRect(FRandomStream& Stream, const FIntPoint& Min, const FIntPoint& Max,
			TArray<TPair<FIntPoint, FIntPoint>>& OutRects)
		{
			const int32 Width = Max.X - Min.X;
			const int32 Depth = Max.Y - Min.Y;
			const int32 Cells = Width * Depth;
			// Rooms are 1-4 cells; split anything bigger.
			if (Cells <= FMath::Max(2, Stream.RandRange(2, 4)) || (Width <= 1 && Depth <= 1))
			{
				OutRects.Add({ Min, Max });
				return;
			}
			const bool bSplitX = Width == Depth ? Stream.RandRange(0, 1) == 0 : Width > Depth;
			if (bSplitX && Width > 1)
			{
				const int32 Cut = Min.X + Stream.RandRange(1, Width - 1);
				SplitRect(Stream, Min, FIntPoint(Cut, Max.Y), OutRects);
				SplitRect(Stream, FIntPoint(Cut, Min.Y), Max, OutRects);
			}
			else if (Depth > 1)
			{
				const int32 Cut = Min.Y + Stream.RandRange(1, Depth - 1);
				SplitRect(Stream, Min, FIntPoint(Max.X, Cut), OutRects);
				SplitRect(Stream, FIntPoint(Min.X, Cut), Max, OutRects);
			}
			else
			{
				OutRects.Add({ Min, Max });
			}
		}

		// Shared wall segment between two cell rects (exclusive-max rects).
		// Returns true + portal center position if they share >= 1 cell edge.
		bool SharedWall(const FFCGenRoom& A, const FFCGenRoom& B, FVector& OutCenter, bool& bOutAlongX)
		{
			// Vertical wall (A right edge == B left edge)?
			if (A.CellMax.X == B.CellMin.X || B.CellMax.X == A.CellMin.X)
			{
				const int32 WallX = A.CellMax.X == B.CellMin.X ? A.CellMax.X : B.CellMax.X;
				const int32 Y0 = FMath::Max(A.CellMin.Y, B.CellMin.Y);
				const int32 Y1 = FMath::Min(A.CellMax.Y, B.CellMax.Y);
				if (Y1 - Y0 >= 1)
				{
					const float MidY = (Y0 + Y1) * 0.5f * CellSize;
					OutCenter = FVector(WallX * CellSize, MidY, 0);
					bOutAlongX = false;
					return true;
				}
			}
			// Horizontal wall (A top edge == B bottom edge)?
			if (A.CellMax.Y == B.CellMin.Y || B.CellMax.Y == A.CellMin.Y)
			{
				const int32 WallY = A.CellMax.Y == B.CellMin.Y ? A.CellMax.Y : B.CellMax.Y;
				const int32 X0 = FMath::Max(A.CellMin.X, B.CellMin.X);
				const int32 X1 = FMath::Min(A.CellMax.X, B.CellMax.X);
				if (X1 - X0 >= 1)
				{
					const float MidX = (X0 + X1) * 0.5f * CellSize;
					OutCenter = FVector(MidX, WallY * CellSize, 0);
					bOutAlongX = true;
					return true;
				}
			}
			return false;
		}
	}

	FFCBuildingData GenerateBuilding(const uint64 GlobalSeed, const int32 BuildingId,
		const FFCBuildingRules& Rules)
	{
		FFCBuildingData Building;
		Building.Seed = GlobalSeed;
		Building.BuildingId = BuildingId;

		// Stage: massing.
		{
			FRandomStream Stream = MakeStream(GlobalSeed, StageId(EStage::Massing), BuildingId);
			Building.FootprintCells.X = Stream.RandRange(Rules.MinFootprint.X, Rules.MaxFootprint.X);
			Building.FootprintCells.Y = Stream.RandRange(Rules.MinFootprint.Y, Rules.MaxFootprint.Y);
			Building.Floors = Stream.RandRange(Rules.MinFloors, Rules.MaxFloors);
		}

		// Stage: floor plans. The stair column occupies the NE corner cell on
		// every floor of a multi-floor building (grid discipline over drama).
		const FIntPoint StairCell(Building.FootprintCells.X - 1, Building.FootprintCells.Y - 1);
		for (int32 Floor = 0; Floor < Building.Floors; ++Floor)
		{
			FRandomStream Stream = MakeStream(GlobalSeed, StageId(EStage::FloorPlans),
				BuildingId * 64 + Floor);
			TArray<TPair<FIntPoint, FIntPoint>> Rects;
			SplitRect(Stream, FIntPoint(0, 0), Building.FootprintCells, Rects);

			for (const TPair<FIntPoint, FIntPoint>& Rect : Rects)
			{
				FFCGenRoom& Room = Building.Rooms.AddDefaulted_GetRef();
				Room.Id = Building.Rooms.Num() - 1;
				Room.Floor = Floor;
				Room.CellMin = Rect.Key;
				Room.CellMax = Rect.Value;
				const bool bHasStairCell = Building.Floors > 1
					&& StairCell.X >= Rect.Key.X && StairCell.X < Rect.Value.X
					&& StairCell.Y >= Rect.Key.Y && StairCell.Y < Rect.Value.Y;
				// Only SMALL rooms around the stair column are stairwells; a
				// large room containing the stairs is still a room (soak
				// finding: all-stairwell floors had nowhere to hide, GEN-07).
				const int32 Area = (Rect.Value.X - Rect.Key.X) * (Rect.Value.Y - Rect.Key.Y);
				Room.Type = (bHasStairCell && Area <= 2) ? ERoomType::Stairwell : ERoomType::Room;
			}
		}

		// Stage: apertures. Interior doors: spanning connections per floor.
		{
			FRandomStream Stream = MakeStream(GlobalSeed, StageId(EStage::Apertures), BuildingId);
			for (int32 Floor = 0; Floor < Building.Floors; ++Floor)
			{
				TArray<int32> FloorRooms;
				for (const FFCGenRoom& Room : Building.Rooms)
				{
					if (Room.Floor == Floor)
					{
						FloorRooms.Add(Room.Id);
					}
				}
				// Spanning tree over floor rooms via shared walls.
				TArray<int32> Connected;
				Connected.Add(FloorRooms[0]);
				bool bProgress = true;
				while (Connected.Num() < FloorRooms.Num() && bProgress)
				{
					bProgress = false;
					for (const int32 Candidate : FloorRooms)
					{
						if (Connected.Contains(Candidate))
						{
							continue;
						}
						for (const int32 Existing : Connected)
						{
							FVector Center;
							bool bAlongX = false;
							if (SharedWall(Building.Rooms[Candidate], Building.Rooms[Existing], Center, bAlongX))
							{
								FFCGenPortal& Portal = Building.Portals.AddDefaulted_GetRef();
								Portal.Id = Building.Portals.Num() - 1;
								Portal.Kind = EGenPortalKind::InteriorDoor;
								Portal.RoomA = Existing;
								Portal.RoomB = Candidate;
								Portal.Position = Center + FVector(0, 0, Floor * FloorHeight + 105.0f);
								Portal.bAlongX = bAlongX;
								Connected.Add(Candidate);
								bProgress = true;
								break;
							}
						}
					}
				}

				// Exterior door: ground floor, south wall of the southernmost room.
				if (Floor == 0)
				{
					int32 BestRoom = FloorRooms[0];
					for (const int32 RoomId : FloorRooms)
					{
						if (Building.Rooms[RoomId].CellMin.Y == 0
							&& Building.Rooms[RoomId].CellMin.X <= Building.Rooms[BestRoom].CellMin.X)
						{
							BestRoom = RoomId;
						}
					}
					const FFCGenRoom& Room = Building.Rooms[BestRoom];
					const float DoorX = (Room.CellMin.X + Room.CellMax.X) * 0.5f * CellSize;
					FFCGenPortal& Portal = Building.Portals.AddDefaulted_GetRef();
					Portal.Id = Building.Portals.Num() - 1;
					Portal.Kind = EGenPortalKind::ExteriorDoor;
					Portal.RoomA = INDEX_NONE;
					Portal.RoomB = BestRoom;
					Portal.Position = FVector(DoorX, 0, 105.0f);
					Portal.bAlongX = true;
				}

				// Windows: one per room on an exterior wall, probability 0.75.
				for (const int32 RoomId : FloorRooms)
				{
					const FFCGenRoom& Room = Building.Rooms[RoomId];
					if (Stream.FRand() > 0.75f)
					{
						continue;
					}
					const float SillZ = Floor * FloorHeight + 90.0f + 65.0f;
					FFCGenPortal Portal;
					Portal.Kind = EGenPortalKind::Window;
					Portal.RoomA = INDEX_NONE;
					Portal.RoomB = RoomId;
					if (Room.CellMin.Y == 0) // south exterior
					{
						Portal.Position = FVector((Room.CellMin.X + Room.CellMax.X) * 0.5f * CellSize, 0, SillZ);
						Portal.bAlongX = true;
					}
					else if (Room.CellMax.Y == Building.FootprintCells.Y) // north
					{
						Portal.Position = FVector((Room.CellMin.X + Room.CellMax.X) * 0.5f * CellSize,
							Building.FootprintCells.Y * CellSize, SillZ);
						Portal.bAlongX = true;
					}
					else if (Room.CellMin.X == 0) // west
					{
						Portal.Position = FVector(0, (Room.CellMin.Y + Room.CellMax.Y) * 0.5f * CellSize, SillZ);
						Portal.bAlongX = false;
					}
					else if (Room.CellMax.X == Building.FootprintCells.X) // east
					{
						Portal.Position = FVector(Building.FootprintCells.X * CellSize,
							(Room.CellMin.Y + Room.CellMax.Y) * 0.5f * CellSize, SillZ);
						Portal.bAlongX = false;
					}
					else
					{
						continue; // interior room: no window
					}
					Portal.Id = Building.Portals.Num();
					Building.Portals.Add(Portal);
				}

				// Stair openings: connect stairwell rooms vertically.
				if (Floor > 0 && Building.Floors > 1)
				{
					// Connect the rooms CONTAINING the stair cell, whatever
					// their type (soak finding: type-based lookup stranded
					// whole floors once big stair-containing rooms stayed
					// ERoomType::Room).
					int32 Lower = INDEX_NONE;
					int32 Upper = INDEX_NONE;
					for (const FFCGenRoom& Room : Building.Rooms)
					{
						const bool bHasStair =
							StairCell.X >= Room.CellMin.X && StairCell.X < Room.CellMax.X
							&& StairCell.Y >= Room.CellMin.Y && StairCell.Y < Room.CellMax.Y;
						if (bHasStair)
						{
							if (Room.Floor == Floor - 1) { Lower = Room.Id; }
							if (Room.Floor == Floor) { Upper = Room.Id; }
						}
					}
					if (Lower != INDEX_NONE && Upper != INDEX_NONE)
					{
						FFCGenPortal& Portal = Building.Portals.AddDefaulted_GetRef();
						Portal.Id = Building.Portals.Num() - 1;
						Portal.Kind = EGenPortalKind::StairOpening;
						Portal.RoomA = Lower;
						Portal.RoomB = Upper;
						Portal.Position = FVector((StairCell.X + 0.5f) * CellSize,
							(StairCell.Y + 0.5f) * CellSize, Floor * FloorHeight);
						Portal.bAlongX = true;
					}
				}
			}
		}

		// Stage: power & lighting. One circuit per floor, one light per room.
		{
			Building.CircuitCount = Building.Floors;
			for (const FFCGenRoom& Room : Building.Rooms)
			{
				FFCGenLight& Light = Building.Lights.AddDefaulted_GetRef();
				Light.Room = Room.Id;
				Light.Circuit = Room.Floor;
				Light.Position = FVector(
					(Room.CellMin.X + Room.CellMax.X) * 0.5f * CellSize,
					(Room.CellMin.Y + Room.CellMax.Y) * 0.5f * CellSize,
					Room.Floor * FloorHeight + FloorHeight - 60.0f);
			}
		}

		// Stage: population. Noise props + one hideable per floor.
		{
			FRandomStream Stream = MakeStream(GlobalSeed, StageId(EStage::Population), BuildingId);
			for (const FFCGenRoom& Room : Building.Rooms)
			{
				if (Room.Type == ERoomType::Stairwell)
				{
					continue;
				}
				const int32 PropCount = Stream.RandRange(1, 3);
				for (int32 Index = 0; Index < PropCount; ++Index)
				{
					FFCGenProp& Prop = Building.Props.AddDefaulted_GetRef();
					Prop.Class = Stream.FRand() < 0.6f ? EPropClass::NoiseSmall : EPropClass::NoiseMedium;
					Prop.Room = Room.Id;
					Prop.Position = FVector(
						FMath::Lerp(Room.CellMin.X * CellSize + 80.0f, Room.CellMax.X * CellSize - 80.0f, Stream.FRand()),
						FMath::Lerp(Room.CellMin.Y * CellSize + 80.0f, Room.CellMax.Y * CellSize - 80.0f, Stream.FRand()),
						Room.Floor * FloorHeight + 40.0f);
				}
			}
			// Hideable: largest non-stair room per floor, corner placement;
			// falls back to ANY room so no floor is ever hide-less (GEN-07).
			for (int32 Floor = 0; Floor < Building.Floors; ++Floor)
			{
				int32 Largest = INDEX_NONE;
				int32 LargestArea = 0;
				for (int32 Pass = 0; Pass < 2 && Largest == INDEX_NONE; ++Pass)
				{
					for (const FFCGenRoom& Room : Building.Rooms)
					{
						const int32 Area = (Room.CellMax.X - Room.CellMin.X) * (Room.CellMax.Y - Room.CellMin.Y);
						const bool bTypeOk = Pass == 1 || Room.Type != ERoomType::Stairwell;
						if (Room.Floor == Floor && bTypeOk && Area > LargestArea)
						{
							Largest = Room.Id;
							LargestArea = Area;
						}
					}
				}
				if (Largest != INDEX_NONE)
				{
					const FFCGenRoom& Room = Building.Rooms[Largest];
					FFCGenProp& Prop = Building.Props.AddDefaulted_GetRef();
					Prop.Class = EPropClass::Hideable;
					Prop.Room = Largest;
					Prop.Position = FVector(Room.CellMin.X * CellSize + 90.0f,
						Room.CellMin.Y * CellSize + 90.0f,
						Room.Floor * FloorHeight + 120.0f);
				}
			}
		}

		return Building;
	}

	TArray<FString> ValidateBuilding(const FFCBuildingData& Building)
	{
		TArray<FString> Failures;

		if (Building.Rooms.Num() == 0)
		{
			Failures.Add(TEXT("GEN-03: no rooms"));
			return Failures;
		}

		// GEN-03: exactly one exterior door on the ground floor.
		int32 ExteriorDoors = 0;
		for (const FFCGenPortal& Portal : Building.Portals)
		{
			if (Portal.Kind == EGenPortalKind::ExteriorDoor)
			{
				++ExteriorDoors;
			}
		}
		if (ExteriorDoors != 1)
		{
			Failures.Add(FString::Printf(TEXT("GEN-03: %d exterior doors (want 1)"), ExteriorDoors));
		}

		// GEN-04: every room reachable from the entrance through door/stair
		// portals (windows do not count as routes).
		{
			TSet<int32> Reachable;
			TArray<int32> Open;
			for (const FFCGenPortal& Portal : Building.Portals)
			{
				if (Portal.Kind == EGenPortalKind::ExteriorDoor && Portal.RoomB != INDEX_NONE)
				{
					Open.Add(Portal.RoomB);
					Reachable.Add(Portal.RoomB);
				}
			}
			while (Open.Num() > 0)
			{
				const int32 Current = Open.Pop();
				for (const FFCGenPortal& Portal : Building.Portals)
				{
					if (Portal.Kind == EGenPortalKind::Window)
					{
						continue;
					}
					int32 Other = INDEX_NONE;
					if (Portal.RoomA == Current) { Other = Portal.RoomB; }
					else if (Portal.RoomB == Current) { Other = Portal.RoomA; }
					if (Other != INDEX_NONE && !Reachable.Contains(Other))
					{
						Reachable.Add(Other);
						Open.Add(Other);
					}
				}
			}
			for (const FFCGenRoom& Room : Building.Rooms)
			{
				if (!Reachable.Contains(Room.Id))
				{
					Failures.Add(FString::Printf(TEXT("GEN-04: room %d (floor %d) unreachable"),
						Room.Id, Room.Floor));
				}
			}
		}

		// Light budget: exactly one light per room, circuit ids valid.
		if (Building.Lights.Num() != Building.Rooms.Num())
		{
			Failures.Add(TEXT("LGT: light count != room count"));
		}
		for (const FFCGenLight& Light : Building.Lights)
		{
			if (Light.Circuit < 0 || Light.Circuit >= Building.CircuitCount)
			{
				Failures.Add(TEXT("LGT: bad circuit id"));
			}
		}

		// Hideable density: at least one per floor (ROADMAP 5.6 check 8).
		for (int32 Floor = 0; Floor < Building.Floors; ++Floor)
		{
			bool bHasHide = false;
			for (const FFCGenProp& Prop : Building.Props)
			{
				if (Prop.Class == EPropClass::Hideable && Building.Rooms[Prop.Room].Floor == Floor)
				{
					bHasHide = true;
					break;
				}
			}
			if (!bHasHide)
			{
				Failures.Add(FString::Printf(TEXT("GEN-07: no hideable on floor %d"), Floor));
			}
		}

		return Failures;
	}

	FRoomGraph BuildRoomGraph(const FFCBuildingData& Building, const FVector& WorldOrigin,
		TArray<int32>& OutPortalIdByGenPortal)
	{
		FRoomGraph Graph;
		// Interior rooms first (point resolution prefers them), street last.
		for (const FFCGenRoom& Room : Building.Rooms)
		{
			Graph.AddRoom(
				WorldOrigin + FVector(Room.CellMin.X * CellSize, Room.CellMin.Y * CellSize, Room.Floor * FloorHeight),
				WorldOrigin + FVector(Room.CellMax.X * CellSize, Room.CellMax.Y * CellSize, (Room.Floor + 1) * FloorHeight));
		}
		const int32 StreetRoom = Graph.AddRoom(
			WorldOrigin + FVector(-4000, -4000, 0),
			WorldOrigin + FVector(Building.FootprintCells.X * CellSize + 4000,
				Building.FootprintCells.Y * CellSize + 4000,
				Building.Floors * FloorHeight));

		OutPortalIdByGenPortal.Init(INDEX_NONE, Building.Portals.Num());
		for (const FFCGenPortal& Portal : Building.Portals)
		{
			const int32 RoomA = Portal.RoomA == INDEX_NONE ? StreetRoom : Portal.RoomA;
			const int32 RoomB = Portal.RoomB == INDEX_NONE ? StreetRoom : Portal.RoomB;
			EAperture State = EAperture::OpenDoorway;
			switch (Portal.Kind)
			{
			case EGenPortalKind::ExteriorDoor: State = EAperture::DoorClosedExterior; break;
			case EGenPortalKind::InteriorDoor: State = EAperture::DoorClosedInterior; break;
			case EGenPortalKind::Window: State = EAperture::WindowOpen; break;
			case EGenPortalKind::StairOpening: State = EAperture::Stairwell; break;
			}
			OutPortalIdByGenPortal[Portal.Id] =
				Graph.AddPortal(RoomA, RoomB, WorldOrigin + Portal.Position, State);
		}
		return Graph;
	}
}
