#include "Perception/FCLightRegistry.h"

#include "Components/LightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarFCLuxDebug(
	TEXT("fc.Lux.Debug"),
	0,
	TEXT("1 = log sampled illumination values (AI-07 agreement tuning)."));

void UFCLightRegistry::RegisterLight(ULightComponent* Light)
{
	if (Light != nullptr)
	{
		Lights.Add(Light);
		UE_LOG(LogTemp, Display, TEXT("[FCLUX] registered %s (%s) intensity=%.1f"),
			*Light->GetOwner()->GetName(), *Light->GetClass()->GetName(), Light->Intensity);
	}
}

float UFCLightRegistry::SampleIllumination(const FVector& Point, const AActor* IgnoreActor) const
{
	// Scalar model, deliberately simple and CPU-cheap; tuned against renders
	// until player intuition transfers (ROADMAP 8.3). Units are gameplay-lux,
	// not photometric truth.
	// Accumulate real lux (candela / m^2), then normalize: ~30 lux (a lit
	// street) saturates to 1.0. Keeps the scale physical and tunable.
	constexpr float SaturationLux = 30.0f;
	float LuxTotal = 0.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FCLux), true);
	if (IgnoreActor != nullptr)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}

	for (const TWeakObjectPtr<ULightComponent>& WeakLight : Lights)
	{
		const ULightComponent* Light = WeakLight.Get();
		if (Light == nullptr || !Light->IsVisible())
		{
			continue;
		}

		const FVector LightPos = Light->GetComponentLocation();
		const float Dist = FVector::Dist(Point, LightPos);

		float Attenuation = 0.0f;
		float ConeFactor = 1.0f;
		if (const ULocalLightComponent* Local = Cast<ULocalLightComponent>(Light))
		{
			const float Radius = Local->AttenuationRadius;
			if (Dist >= Radius)
			{
				continue;
			}
			// Inverse-square with a windowed falloff to zero at the radius.
			const float DistM = FMath::Max(Dist / 100.0f, 0.5f);
			const float Window = FMath::Square(1.0f - FMath::Square(Dist / Radius));
			Attenuation = Window / (DistM * DistM);

			if (const USpotLightComponent* Spot = Cast<USpotLightComponent>(Light))
			{
				const FVector ToPoint = (Point - LightPos).GetSafeNormal();
				const float CosAngle = FVector::DotProduct(Spot->GetForwardVector(), ToPoint);
				const float CosOuter = FMath::Cos(FMath::DegreesToRadians(Spot->OuterConeAngle));
				const float CosInner = FMath::Cos(FMath::DegreesToRadians(Spot->InnerConeAngle));
				if (CosAngle <= CosOuter)
				{
					if (CVarFCLuxDebug.GetValueOnGameThread() >= 2)
					{
						UE_LOG(LogTemp, Display, TEXT("[FCLUX]   %s OUT OF CONE cos=%.3f outer=%.3f fwd=%s"),
							*Light->GetOwner()->GetName(), CosAngle, CosOuter,
							*Spot->GetForwardVector().ToCompactString());
					}
					continue;
				}
				ConeFactor = FMath::Clamp((CosAngle - CosOuter) / FMath::Max(CosInner - CosOuter, 0.01f), 0.0f, 1.0f);
			}
		}
		else
		{
			// Directional (moon): its intensity IS lux; sky-occlusion trace.
			FHitResult Hit;
			const FVector SkyDir = -Light->GetDirection();
			if (GetWorld()->LineTraceSingleByChannel(Hit, Point, Point + SkyDir * 5000.0f, ECC_Visibility, Params))
			{
				continue; // under a roof: no moon
			}
			LuxTotal += Light->Intensity;
			continue;
		}

		// Occlusion: one trace. (Cached-per-N-frames arrives with the AI
		// tick budget pass if the profiler asks for it.)
		FHitResult Hit;
		if (GetWorld()->LineTraceSingleByChannel(Hit, LightPos, Point, ECC_Visibility, Params))
		{
			if (CVarFCLuxDebug.GetValueOnGameThread() != 0)
			{
				UE_LOG(LogTemp, Display, TEXT("[FCLUX] blocked by %s (%s) at %s"),
					Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("<null>"),
					Hit.GetComponent() ? *Hit.GetComponent()->GetName() : TEXT("<null>"),
					*Hit.ImpactPoint.ToCompactString());
			}
			continue; // blocked
		}

		// Candela / m^2 = lux at the point.
		LuxTotal += Light->Intensity * Attenuation * ConeFactor;
		if (CVarFCLuxDebug.GetValueOnGameThread() >= 2)
		{
			UE_LOG(LogTemp, Display, TEXT("[FCLUX]   %s dist=%.0f atten=%.4f cone=%.2f -> +%.1f lux"),
				*Light->GetOwner()->GetName(), Dist, Attenuation, ConeFactor,
				Light->Intensity * Attenuation * ConeFactor);
		}
	}

	const float Illumination = FMath::Clamp(LuxTotal / SaturationLux, 0.0f, 1.0f);
	if (CVarFCLuxDebug.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogTemp, Display, TEXT("[FCLUX] %.3f at %s"), Illumination, *Point.ToCompactString());
	}
	return Illumination;
}

void UFCLightRegistry::NotifyLightStateChanged(const FVector& Location)
{
	FLightChange& Change = RecentChanges.AddDefaulted_GetRef();
	Change.Location = Location;
	Change.Time = GetWorld()->GetTimeSeconds();
	if (RecentChanges.Num() > 32)
	{
		RecentChanges.RemoveAt(0);
	}
}

float UFCLightRegistry::TimeSinceNearbyLightChange(const FVector& Point, const float Radius) const
{
	const float Now = GetWorld()->GetTimeSeconds();
	float Best = BIG_NUMBER;
	for (const FLightChange& Change : RecentChanges)
	{
		if (FVector::Dist(Change.Location, Point) <= Radius)
		{
			Best = FMath::Min(Best, Now - Change.Time);
		}
	}
	return Best;
}

void UFCLightRegistry::SetActiveBeam(AActor* Owner, const FVector& Origin,
	const FVector& Direction, const float Length, const bool bActive)
{
	Beams.RemoveAll([Owner](const FBeam& Beam) { return Beam.Owner.Get() == Owner || !Beam.Owner.IsValid(); });
	if (bActive)
	{
		FBeam& Beam = Beams.AddDefaulted_GetRef();
		Beam.Owner = Owner;
		Beam.Origin = Origin;
		Beam.Direction = Direction;
		Beam.Length = Length;
	}
}
