#include "Noise/FCNoiseSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarFCNoiseDebug(
	TEXT("fc.Noise.Debug"),
	0,
	TEXT("1 = draw noise events as expanding spheres and log them."));

void UFCNoiseSubsystem::SetPortalState(const int32 PortalId, const FC::Gen::EAperture State)
{
	if (RoomGraph.Portals.IsValidIndex(PortalId))
	{
		RoomGraph.Portals[PortalId].State = State;
	}
}

float UFCNoiseSubsystem::PerceivedLoudnessAt(const FFCNoiseEvent& Event,
	const FVector& ListenerLocation) const
{
	// No topology registered: plain distance falloff so hearing still works
	// in bare test worlds.
	constexpr float DistanceLossPerMeter = 0.55f;
	if (RoomGraph.Rooms.Num() == 0)
	{
		return FMath::Max(Event.Loudness
			- FVector::Dist(Event.Origin, ListenerLocation) / 100.0f * DistanceLossPerMeter, 0.0f);
	}

	const int32 OriginRoom = RoomGraph.ResolveRoomOrNearest(Event.Origin);
	const int32 ListenerRoom = RoomGraph.ResolveRoomOrNearest(ListenerLocation);
	if (OriginRoom == ListenerRoom)
	{
		return FMath::Max(Event.Loudness
			- FVector::Dist(Event.Origin, ListenerLocation) / 100.0f * DistanceLossPerMeter, 0.0f);
	}

	const FC::Gen::FPropagationResult Result = FC::Gen::PropagateNoise(
		RoomGraph, OriginRoom, Event.Origin, Event.Loudness);
	return Result.LoudnessPerRoom.IsValidIndex(ListenerRoom)
		? Result.LoudnessPerRoom[ListenerRoom]
		: 0.0f;
}

void UFCNoiseSubsystem::EmitNoise(const FVector& Origin, const float Loudness,
	const FName SourceTag, AActor* Instigator)
{
	FFCNoiseEvent Event;
	Event.Origin = Origin;
	Event.Loudness = Loudness;
	Event.SourceTag = SourceTag;
	Event.Instigator = Instigator;
	Event.Timestamp = GetWorld()->GetTimeSeconds();

	RecentEvents.Add(Event);
	if (RecentEvents.Num() > 64)
	{
		RecentEvents.RemoveAt(0);
	}

	OnNoiseEmitted.Broadcast(Event);

	if (CVarFCNoiseDebug.GetValueOnGameThread() != 0)
	{
		// Sphere marks the event; per-room loudness draws the propagation
		// truth the AI actually hears (`show` cheat of ROADMAP 7.4).
		DrawDebugSphere(GetWorld(), Origin, Loudness * 8.0f, 12,
			FColor::Orange, false, 1.2f, 0, 1.5f);
		if (RoomGraph.Rooms.Num() > 0)
		{
			const int32 OriginRoom = RoomGraph.ResolveRoomOrNearest(Origin);
			const FC::Gen::FPropagationResult Result = FC::Gen::PropagateNoise(
				RoomGraph, OriginRoom, Origin, Loudness);
			for (const FC::Gen::FRoom& Room : RoomGraph.Rooms)
			{
				DrawDebugString(GetWorld(), Room.Center,
					FString::Printf(TEXT("%.0f"), Result.LoudnessPerRoom[Room.Id]),
					nullptr, FColor::Cyan, 1.2f);
			}
		}
		UE_LOG(LogFootcandle, Display, TEXT("[FCNOISE] %s loudness=%.0f at %s"),
			*SourceTag.ToString(), Loudness, *Origin.ToCompactString());
	}
}
