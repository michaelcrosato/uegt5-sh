#include "World/FCLightFixture.h"

#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Perception/FCLightRegistry.h"
#include "Player/FCPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const TCHAR* FixtureCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* FixtureCylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	const TCHAR* FixtureSpherePath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	const TCHAR* FixtureMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
}

AFCLightFixture::AFCLightFixture()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Housing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Housing"));
	Housing->SetupAttachment(Root);
	Housing->SetMobility(EComponentMobility::Movable);

	Bulb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bulb"));
	Bulb->SetupAttachment(Root);
	Bulb->SetMobility(EComponentMobility::Movable);
	Bulb->SetNotifyRigidBodyCollision(true); // thrown props can shatter it
	Bulb->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AFCLightFixture::Configure(const EFCFixtureStyle InStyle, const FLinearColor& Color,
	const float IntensityCandela, const float AttenuationRadius,
	const EFCFlickerStyle FlickerStyle, const bool bWithFlicker, const uint64 FlickerSeed)
{
	Style = InStyle;
	OnColor = Color;

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, FixtureCubePath);
	UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, FixtureCylinderPath);
	UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, FixtureSpherePath);

	// The visible body per style. RULE: the bulb mesh sits just ABOVE/BESIDE
	// its light point, never around it - a mesh enclosing the light origin
	// blocks its own RT shadows AND the registry's occlusion trace (the M4
	// dead-lux failure shape). Placed close, the real light blazes the mesh
	// at point-blank inverse-square range: that is what makes "on" read hot
	// with a plain base-color material.
	switch (Style)
	{
	case EFCFixtureStyle::CeilingBulb:
	{
		// Bulb sphere just above the light point; cord up into the slab.
		Housing->SetStaticMesh(Cylinder);
		Housing->SetRelativeScale3D(FVector(0.04f, 0.04f, 0.40f));
		Housing->SetRelativeLocation(FVector(0, 0, 36.0f));
		Bulb->SetStaticMesh(Sphere);
		Bulb->SetRelativeScale3D(FVector(0.16f));
		Bulb->SetRelativeLocation(FVector(0, 0, 8.0f));
		UPointLightComponent* Point = NewObject<UPointLightComponent>(this, TEXT("Point"));
		Point->SetupAttachment(Root);
		Point->SetRelativeLocation(FVector(0, 0, -6.0f));
		Point->RegisterComponent();
		Light = Point;
		break;
	}
	case EFCFixtureStyle::Streetlight:
	{
		// Cobra head: pole 80cm back, head arm reaching over the light so
		// neither mesh sits between the light point and the street.
		Housing->SetStaticMesh(Cylinder);
		Housing->SetRelativeScale3D(FVector(0.14f, 0.14f, 5.5f));
		Housing->SetRelativeLocation(FVector(-80.0f, 0, -275.0f));
		Bulb->SetStaticMesh(Cube); // arm + head, one breakable piece
		Bulb->SetRelativeScale3D(FVector(1.0f, 0.32f, 0.14f));
		Bulb->SetRelativeLocation(FVector(-40.0f, 0, 8.0f));
		USpotLightComponent* Spot = NewObject<USpotLightComponent>(this, TEXT("Spot"));
		Spot->SetupAttachment(Root);
		Spot->SetRelativeLocation(FVector(0, 0, -6.0f));
		Spot->RegisterComponent();
		Spot->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f));
		Spot->SetInnerConeAngle(28.0f);
		Spot->SetOuterConeAngle(46.0f);
		Spot->SetVolumetricScatteringIntensity(2.0f);
		Light = Spot;
		break;
	}
	case EFCFixtureStyle::TV:
	{
		Housing->SetStaticMesh(Cube); // the set
		Housing->SetRelativeScale3D(FVector(0.5f, 0.16f, 0.36f));
		Housing->SetRelativeLocation(FVector(0, 0, 0));
		Bulb->SetStaticMesh(Cube); // the screen, proud of the face
		Bulb->SetRelativeScale3D(FVector(0.42f, 0.02f, 0.30f));
		Bulb->SetRelativeLocation(FVector(0, -9.0f, 0));
		UPointLightComponent* Point = NewObject<UPointLightComponent>(this, TEXT("Point"));
		Point->SetupAttachment(Root);
		Point->SetRelativeLocation(FVector(0, -25.0f, 0)); // in front: lights its own screen
		Point->RegisterComponent();
		Light = Point;
		break;
	}
	case EFCFixtureStyle::EmergencyLED:
	{
		Housing->SetStaticMesh(Cube); // little wall box
		Housing->SetRelativeScale3D(FVector(0.10f, 0.10f, 0.06f));
		Bulb->SetStaticMesh(Sphere); // LED dome just above the light point
		Bulb->SetRelativeScale3D(FVector(0.05f));
		Bulb->SetRelativeLocation(FVector(0, 0, 11.0f));
		UPointLightComponent* Point = NewObject<UPointLightComponent>(this, TEXT("Point"));
		Point->SetupAttachment(Root);
		Point->SetRelativeLocation(FVector(0, 0, 6.0f));
		Point->RegisterComponent();
		Light = Point;
		break;
	}
	}

	Light->SetMobility(EComponentMobility::Movable);
	if (UPointLightComponent* AsLocal = Cast<UPointLightComponent>(Light.Get()))
	{
		AsLocal->SetIntensityUnits(ELightUnits::Candelas);
		AsLocal->SetAttenuationRadius(AttenuationRadius);
	}
	else if (USpotLightComponent* AsSpot = Cast<USpotLightComponent>(Light.Get()))
	{
		AsSpot->SetIntensityUnits(ELightUnits::Candelas);
		AsSpot->SetAttenuationRadius(AttenuationRadius);
	}
	Light->SetIntensity(IntensityCandela);
	Light->SetLightColor(Color);

	// Bulb material: color-driven MID; the light itself does the glowing.
	if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, FixtureMaterialPath))
	{
		BulbMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Bulb->SetMaterial(0, BulbMID);
	}
	Bulb->OnComponentHit.AddDynamic(this, &AFCLightFixture::OnBulbHit);

	if (bWithFlicker)
	{
		Flicker = NewObject<UFCFlickerComponent>(this, TEXT("Flicker"));
		Flicker->RegisterComponent();
		Flicker->Configure(Light, FlickerStyle, FlickerSeed);
	}

	if (UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
	{
		Registry->RegisterLight(Light);
	}
	SyncBulbAppearance();
}

ULightComponent* AFCLightFixture::GetLightComponent() const
{
	return Light;
}

bool AFCLightFixture::IsOn() const
{
	return !bBroken && Light != nullptr && Light->IsVisible();
}

void AFCLightFixture::SyncBulbAppearance()
{
	if (BulbMID == nullptr)
	{
		return;
	}
	if (bBroken)
	{
		BulbMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.02f, 0.02f, 0.02f));
	}
	else if (IsOn())
	{
		// Well over 1.0: reads hot even before its own light lands on it.
		BulbMID->SetVectorParameterValue(TEXT("Color"), OnColor * 3.0f);
	}
	else
	{
		BulbMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.08f, 0.08f, 0.08f));
	}
}

void AFCLightFixture::Break(AActor* Breaker)
{
	if (bBroken)
	{
		return;
	}
	bBroken = true;
	if (Light != nullptr)
	{
		Light->SetVisibility(false);
	}
	if (Flicker != nullptr)
	{
		Flicker->DestroyComponent(); // a dead bulb does not sputter
		Flicker = nullptr;
	}
	// A shattering bulb is district-loud glass and a light delta at once.
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->EmitNoise(GetActorLocation(), 95.0f, TEXT("Noise.Source.Glass"), Breaker);
	}
	if (UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
	{
		Registry->NotifyLightStateChanged(GetActorLocation());
	}
	// The body shows it: bulb askew and dark.
	Bulb->AddRelativeRotation(FRotator(24.0f, 15.0f, 0));
	Bulb->AddRelativeLocation(FVector(0, 0, -3.0f));
	SyncBulbAppearance();
	UE_LOG(LogFootcandle, Display, TEXT("[FCFIXTURE] %s BROKEN"), *GetName());
}

void AFCLightFixture::OnBulbHit(UPrimitiveComponent* /*HitComponent*/, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, const FVector NormalImpulse, const FHitResult& /*Hit*/)
{
	// Only a real impact shatters glass - resting contacts don't. Impulse is
	// kg*cm/s: a ~2kg bottle thrown at speed lands ~1300+, resting jitter ~30.
	if (!bBroken && OtherComp != nullptr && OtherComp->IsSimulatingPhysics()
		&& NormalImpulse.Size() > 400.0f)
	{
		Break(OtherActor);
	}
}

void AFCLightFixture::Interact(AFCPlayerCharacter* User, const bool /*bQuiet*/)
{
	if (bBroken || Light == nullptr)
	{
		return;
	}
	Light->SetVisibility(!Light->IsVisible());
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->EmitNoise(GetActorLocation(), 6.0f, TEXT("Noise.Source.Switch"), User);
	}
	if (UFCLightRegistry* Registry = GetWorld()->GetSubsystem<UFCLightRegistry>())
	{
		Registry->NotifyLightStateChanged(GetActorLocation());
	}
	SyncBulbAppearance();
}

FString AFCLightFixture::GetInteractionVerb() const
{
	if (bBroken)
	{
		return TEXT("Broken");
	}
	return IsOn() ? TEXT("Light off") : TEXT("Light on");
}

void AFCLightFixture::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// External controllers (wall switches, breakers, the grid) toggle the
	// light component directly - keep the visible bulb in sync, cheaply.
	SyncAccumulator += DeltaSeconds;
	if (SyncAccumulator >= 0.2f)
	{
		SyncAccumulator = 0.0f;
		const bool bVisibleNow = IsOn();
		if (bVisibleNow != bLastVisibleState)
		{
			bLastVisibleState = bVisibleNow;
			SyncBulbAppearance();
		}
	}
}
