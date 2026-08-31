#include "World/FCBreakerPanel.h"

#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Objectives/FCRunSubsystem.h"
#include "Perception/FCLightRegistry.h"
#include "Player/FCPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

AFCBreakerPanel::AFCBreakerPanel()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Box"));
	SetRootComponent(Box);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Box->SetStaticMesh(CubeMesh.Object);
	}
	Box->SetRelativeScale3D(FVector(0.16f, 0.45f, 0.65f));
	Box->SetMobility(EComponentMobility::Movable);
}

void AFCBreakerPanel::LinkLight(ULightComponent* Light)
{
	if (Light != nullptr)
	{
		LinkedLights.Add(Light);
		Light->SetVisibility(bOn);
	}
}

void AFCBreakerPanel::Interact(AFCPlayerCharacter* User, const bool /*bQuiet*/)
{
	bOn = !bOn;
	FVector DeltaCenter = GetActorLocation();
	for (const TWeakObjectPtr<ULightComponent>& Light : LinkedLights)
	{
		if (Light.IsValid())
		{
			Light->SetVisibility(bOn);
			DeltaCenter = Light->GetComponentLocation();
		}
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCGRID] %s -> %s (%d lights)"),
		*Label, bOn ? TEXT("ON") : TEXT("OFF"), LinkedLights.Num());

	// A breaker throw is loud AND a light delta - both hunters' channels.
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->EmitNoise(GetActorLocation(), 70.0f, TEXT("Noise.Source.Breaker"), User);
	}
	if (UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
	{
		Registry->NotifyLightStateChanged(DeltaCenter);
	}
	if (bSatisfiesConditionWhenOn && bOn && !bConditionReported)
	{
		bConditionReported = true;
		if (UFCRunSubsystem* Run = GetWorld()->GetSubsystem<UFCRunSubsystem>())
		{
			Run->NotifyConditionSatisfied(TEXT("power restored"));
		}
	}
}

FString AFCBreakerPanel::GetInteractionVerb() const
{
	return bOn ? FString::Printf(TEXT("%s off"), *Label) : FString::Printf(TEXT("%s on"), *Label);
}
