#include "FCRoomGraph.h"

namespace FC::Gen
{
	float ApertureLoss(const EAperture Aperture)
	{
		// ROADMAP 7.2 loss table. Tunable; the ORDER is the design.
		switch (Aperture)
		{
		case EAperture::OpenDoorway: return 3.0f;
		case EAperture::DoorOpen: return 5.0f;
		case EAperture::DoorClosedInterior: return 22.0f;
		case EAperture::DoorClosedExterior: return 30.0f;
		case EAperture::WindowOpen: return 6.0f;
		case EAperture::WindowClosed: return 25.0f;
		case EAperture::WindowBroken: return 4.0f;
		case EAperture::Stairwell: return 4.0f;
		case EAperture::FloorCeiling: return 35.0f;
		case EAperture::Wall: return 45.0f;
		default: return 45.0f;
		}
	}

	int32 FRoomGraph::AddRoom(const FVector& BoundsMin, const FVector& BoundsMax)
	{
		FRoom& Room = Rooms.AddDefaulted_GetRef();
		Room.Id = Rooms.Num() - 1;
		Room.BoundsMin = BoundsMin;
		Room.BoundsMax = BoundsMax;
		Room.Center = (BoundsMin + BoundsMax) * 0.5f;
		return Room.Id;
	}

	int32 FRoomGraph::AddPortal(const int32 RoomA, const int32 RoomB,
		const FVector& Location, const EAperture State)
	{
		check(Rooms.IsValidIndex(RoomA) && Rooms.IsValidIndex(RoomB));
		FPortal& Portal = Portals.AddDefaulted_GetRef();
		Portal.Id = Portals.Num() - 1;
		Portal.RoomA = RoomA;
		Portal.RoomB = RoomB;
		Portal.Location = Location;
		Portal.State = State;
		return Portal.Id;
	}

	int32 FRoomGraph::ResolveRoom(const FVector& Point) const
	{
		for (const FRoom& Room : Rooms)
		{
			if (Point.X >= Room.BoundsMin.X && Point.X <= Room.BoundsMax.X
				&& Point.Y >= Room.BoundsMin.Y && Point.Y <= Room.BoundsMax.Y
				&& Point.Z >= Room.BoundsMin.Z && Point.Z <= Room.BoundsMax.Z)
			{
				return Room.Id;
			}
		}
		return INDEX_NONE;
	}

	int32 FRoomGraph::ResolveRoomOrNearest(const FVector& Point) const
	{
		const int32 Direct = ResolveRoom(Point);
		if (Direct != INDEX_NONE)
		{
			return Direct;
		}
		int32 Best = INDEX_NONE;
		float BestDistSq = TNumericLimits<float>::Max();
		for (const FRoom& Room : Rooms)
		{
			const float DistSq = FVector::DistSquared(Point, Room.Center);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Room.Id;
			}
		}
		return Best;
	}

	FPropagationResult PropagateNoise(const FRoomGraph& Graph, const int32 OriginRoom,
		const FVector& Origin, const float Loudness, const float CutoffLoudness,
		const int32 MaxVisitedRooms, const float DistanceLossPerMeter)
	{
		FPropagationResult Result;
		Result.LoudnessPerRoom.Init(0.0f, Graph.Rooms.Num());
		if (!Graph.Rooms.IsValidIndex(OriginRoom) || Loudness <= 0.0f)
		{
			return Result;
		}

		// Adjacency (rebuilt per call - graphs are small; cache later if the
		// profiler ever cares. Determinism: arrays, stable order, no TMap
		// iteration affecting results).
		TArray<TArray<int32>> PortalsByRoom;
		PortalsByRoom.SetNum(Graph.Rooms.Num());
		for (const FPortal& Portal : Graph.Portals)
		{
			PortalsByRoom[Portal.RoomA].Add(Portal.Id);
			PortalsByRoom[Portal.RoomB].Add(Portal.Id);
		}

		// Dijkstra over "loss" (origin loudness - arriving loudness).
		// Seed: distance from the actual origin point to each portal of the
		// origin room, so a noise beside a doorway leaks more than one in the
		// far corner.
		TArray<float> BestLoudness;
		BestLoudness.Init(-1.0f, Graph.Rooms.Num());
		BestLoudness[OriginRoom] = Loudness;

		struct FEntry
		{
			int32 Room;
			float Loudness;
			FVector EntryPoint; // where the sound "entered" this room
		};
		TArray<FEntry> Open;
		Open.Add({ OriginRoom, Loudness, Origin });
		int32 Visited = 0;

		while (Open.Num() > 0 && Visited < MaxVisitedRooms)
		{
			// Pop max-loudness entry (linear scan; graphs are tens of rooms).
			int32 BestIndex = 0;
			for (int32 Index = 1; Index < Open.Num(); ++Index)
			{
				if (Open[Index].Loudness > Open[BestIndex].Loudness)
				{
					BestIndex = Index;
				}
			}
			const FEntry Current = Open[BestIndex];
			Open.RemoveAtSwap(BestIndex);
			if (Current.Loudness < BestLoudness[Current.Room])
			{
				continue; // stale
			}
			++Visited;

			for (const int32 PortalId : PortalsByRoom[Current.Room])
			{
				const FPortal& Portal = Graph.Portals[PortalId];
				const int32 Other = Portal.RoomA == Current.Room ? Portal.RoomB : Portal.RoomA;
				const float DistMeters = FVector::Dist(Current.EntryPoint, Portal.Location) / 100.0f;
				const float Arriving = Current.Loudness
					- DistMeters * DistanceLossPerMeter
					- ApertureLoss(Portal.State);
				if (Arriving > CutoffLoudness && Arriving > BestLoudness[Other])
				{
					BestLoudness[Other] = Arriving;
					Open.Add({ Other, Arriving, Portal.Location });
				}
			}
		}

		for (int32 RoomId = 0; RoomId < BestLoudness.Num(); ++RoomId)
		{
			Result.LoudnessPerRoom[RoomId] = FMath::Max(BestLoudness[RoomId], 0.0f);
		}
		return Result;
	}
}
