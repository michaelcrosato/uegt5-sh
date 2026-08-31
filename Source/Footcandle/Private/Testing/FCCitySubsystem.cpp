#include "Testing/FCCitySubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
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
#include "Noise/FCNoiseSubsystem.h"
#include "Perception/FCLightRegistry.h"
#include "Testing/FCTestStationSubsystem.h"

using namespace FC::Gen;

void UFCCitySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	FString SeedString;
	if (FParse::Value(FCommandLine::Get(), TEXT("fccity="), SeedString))
	{
		SpawnFromSeed(FCString::Strtoui64(*SeedString, nullptr, 10));
	}
}

void UFCCitySubsystem::Deinitialize()
{
	if (StreamTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(StreamTicker);
	}
	Super::Deinitialize();
}

int32 UFCCitySubsystem::CountLotsWithShell() const
{
	int32 Count = 0;
	for (const FFCSpawnedBuilding& Lot : SpawnedLots)
	{
		if (Lot.ShellActors.Num() > 0)
		{
			++Count;
		}
	}
	return Count;
}

bool UFCCitySubsystem::SpawnFromSeed(const uint64 Seed)
{
	if (bSpawned)
	{
		return false;
	}

	City = GenerateCity(Seed, FFCCityRules());
	const TArray<FString> Problems = ValidateCity(City);
	if (Problems.Num() > 0)
	{
		UE_LOG(LogFootcandle, Error, TEXT("[FCCITY] seed %llu INVALID: %s"),
			Seed, *FString::Join(Problems, TEXT("; ")));
		return false;
	}
	bSpawned = true;
	UWorld* World = GetWorld();

	UE_LOG(LogFootcandle, Display, TEXT("[FCCITY] seed %llu: %dx%d lots, %d street lights"),
		Seed, City.LotGrid.X, City.LotGrid.Y, City.StreetLightPositions.Num());

	// --- Ground, moon, exposure ---
	{
		UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Min(City.ExtentMin.X - 2000, City.ExtentMin.Y - 2000, -20);
		const FVector Max(City.ExtentMax.X + 2000, City.ExtentMax.Y + 2000, 0);
		if (AStaticMeshActor* Ground = World->SpawnActor<AStaticMeshActor>(
			(Min + Max) * 0.5f, FRotator::ZeroRotator, Params))
		{
			Ground->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Ground->GetStaticMeshComponent()->SetStaticMesh(Cube);
			Ground->SetActorScale3D((Max - Min) / 100.0f);
		}
		if (ADirectionalLight* Moon = World->SpawnActor<ADirectionalLight>(FVector(0, 0, 2000), FRotator::ZeroRotator))
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

	// --- Street lights: the sodium grid (all MegaLights, all shadowed) ---
	{
		UFCLightRegistry* Registry = World->GetSubsystem<UFCLightRegistry>();
		for (const FVector& LightPos : City.StreetLightPositions)
		{
			ASpotLight* Street = World->SpawnActor<ASpotLight>(LightPos, FRotator::ZeroRotator);
			if (Street != nullptr)
			{
				Street->GetLightComponent()->SetMobility(EComponentMobility::Movable);
				USpotLightComponent* Component = CastChecked<USpotLightComponent>(Street->GetLightComponent());
				Component->SetWorldRotation(FRotator(-90.0f, 0.0f, 0.0f)); // straight down
				Component->SetIntensityUnits(ELightUnits::Candelas);
				Component->SetIntensity(700.0f);
				Component->SetLightColor(FLinearColor(1.0f, 0.64f, 0.23f));
				Component->SetAttenuationRadius(1800.0f);
				Component->SetInnerConeAngle(28.0f);
				Component->SetOuterConeAngle(46.0f);
				Component->SetVolumetricScatteringIntensity(2.0f);
				if (Registry != nullptr)
				{
					Registry->RegisterLight(Component);
				}
			}
		}
	}

	// --- Shells for every lot (the skyline tier) ---
	SpawnedLots.SetNum(City.Lots.Num());
	for (int32 LotIndex = 0; LotIndex < City.Lots.Num(); ++LotIndex)
	{
		FC::Spawn::SpawnShell(World, City.Lots[LotIndex].Building,
			City.Lots[LotIndex].Origin, SpawnedLots[LotIndex]);
	}

	// --- Stations ---
	if (UFCTestStationSubsystem* Stations = World->GetSubsystem<UFCTestStationSubsystem>())
	{
		const FVector Center(
			(City.ExtentMin.X + City.ExtentMax.X) * 0.5f,
			(City.ExtentMin.Y + City.ExtentMax.Y) * 0.5f, 0);
		Stations->RegisterStation(TEXT("CityOverview"),
			Center + FVector(0, -5200, 2600), FRotator(-26.0f, 90.0f, 0.0f));
		Stations->RegisterStation(TEXT("CityStreet"),
			FVector(Center.X, -StreetWidth * 0.5f, 165), FRotator(0.0f, 30.0f, 0.0f));
		Stations->RegisterStation(TEXT("CityAlley"),
			FVector(LotPitch - StreetWidth * 0.5f, Center.Y, 165), FRotator(0.0f, 90.0f, 0.0f));
	}

	// Player: south street, mid-city.
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			const float MidX = (City.ExtentMin.X + City.ExtentMax.X) * 0.5f;
			Pawn->TeleportTo(FVector(MidX, -StreetWidth * 0.5f, 120), FRotator(0, 90, 0), false, true);
			PC->SetControlRotation(FRotator(0, 90, 0));
		}
	}

	StreamTicker = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCCitySubsystem::TickStreamer), 0.5f);
	return true;
}

bool UFCCitySubsystem::TickStreamer(float /*DeltaTime*/)
{
	UWorld* World = GetWorld();
	const APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Pawn = PC != nullptr ? PC->GetPawn() : nullptr;
	if (Pawn == nullptr)
	{
		return true;
	}
	const FVector PlayerPos = Pawn->GetActorLocation();

	// Nearest lot by building center, within the detail radius.
	int32 Nearest = INDEX_NONE;
	float NearestDist = DetailRadius;
	for (int32 LotIndex = 0; LotIndex < City.Lots.Num(); ++LotIndex)
	{
		const FFCCityLot& Lot = City.Lots[LotIndex];
		const FVector Center = Lot.Origin + FVector(
			Lot.Building.FootprintCells.X * CellSize * 0.5f,
			Lot.Building.FootprintCells.Y * CellSize * 0.5f, 0);
		const float Dist = FVector::Dist2D(PlayerPos, Center);
		if (Dist < NearestDist)
		{
			Nearest = LotIndex;
			NearestDist = Dist;
		}
	}

	if (Nearest == DetailLot)
	{
		return true;
	}

	// Swap: despawn old detail, stream the new interior + its room graph.
	if (DetailLot != INDEX_NONE)
	{
		FC::Spawn::DespawnDetail(SpawnedLots[DetailLot]);
	}
	DetailLot = Nearest;
	if (UFCNoiseSubsystem* Noise = World->GetSubsystem<UFCNoiseSubsystem>())
	{
		if (DetailLot != INDEX_NONE)
		{
			Noise->GetRoomGraph() = BuildRoomGraph(City.Lots[DetailLot].Building,
				City.Lots[DetailLot].Origin, SpawnedLots[DetailLot].GraphPortalByGenPortal);
		}
		else
		{
			Noise->GetRoomGraph() = FRoomGraph();
		}
	}
	if (DetailLot != INDEX_NONE)
	{
		FC::Spawn::SpawnDetail(World, City.Lots[DetailLot].Building,
			City.Lots[DetailLot].Origin, SpawnedLots[DetailLot]);
		UE_LOG(LogFootcandle, Display, TEXT("[FCCITY] interior streamed: lot %d (dist %.0f)"),
			DetailLot, NearestDist);
	}
	return true;
}
