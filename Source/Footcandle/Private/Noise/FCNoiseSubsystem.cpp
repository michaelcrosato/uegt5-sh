#include "Noise/FCNoiseSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarFCNoiseDebug(
	TEXT("fc.Noise.Debug"),
	0,
	TEXT("1 = draw noise events as expanding spheres and log them."));

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
		// Radius scaled for readability only; real audibility is M3's
		// portal-graph propagation, not a sphere.
		DrawDebugSphere(GetWorld(), Origin, Loudness * 8.0f, 12,
			FColor::Orange, false, 1.2f, 0, 1.5f);
		UE_LOG(LogFootcandle, Display, TEXT("[FCNOISE] %s loudness=%.0f at %s"),
			*SourceTag.ToString(), Loudness, *Origin.ToCompactString());
	}
}
