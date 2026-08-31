#include "World/FCLightSwitch.h"

#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Player/FCPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

AFCLightSwitch::AFCLightSwitch()
{
	PrimaryActorTick.bCanEverTick = false;

	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
	SetRootComponent(Plate);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Plate->SetStaticMesh(CubeMesh.Object);
	}
	Plate->SetRelativeScale3D(FVector(0.08f, 0.12f, 0.18f));
	Plate->SetMobility(EComponentMobility::Movable);
}

void AFCLightSwitch::LinkLight(ULightComponent* Light)
{
	LinkedLights.Add(Light);
}

void AFCLightSwitch::Interact(AFCPlayerCharacter* User, const bool /*bQuiet*/)
{
	bOn = !bOn;
	for (const TWeakObjectPtr<ULightComponent>& Light : LinkedLights)
	{
		if (Light.IsValid())
		{
			Light->SetVisibility(bOn);
		}
	}
	// A switch is quiet - but it is not silent, and the light *change* is the
	// loud part to anything that hunts by light (the Watcher, M4).
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->EmitNoise(GetActorLocation(), 6.0f, TEXT("Noise.Source.Switch"), User);
	}
}

FString AFCLightSwitch::GetInteractionVerb() const
{
	return bOn ? TEXT("Lights off") : TEXT("Lights on");
}
