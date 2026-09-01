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
#include "AI/FCDirectorSubsystem.h"
#include "AI/FCListener.h"
#include "Footcandle.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Objectives/FCExtractZone.h"
#include "Objectives/FCKeyItem.h"
#include "Objectives/FCRunSubsystem.h"
#include "Perception/FCLightRegistry.h"
#include "Testing/FCTestStationSubsystem.h"
#include "World/FCBreakerPanel.h"

using namespace FC::Gen;

namespace
{
	// The auto-run seed persists across in-process restarts (R after death:
	// "same seed, same city - it will be waiting"). A fresh launch rolls new.
	uint64 GAutoRunSeed = 0;
}

void UFCCitySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld())
	{
		return;
	}
	const TCHAR* CommandLine = FCommandLine::Get();
	FString SeedString;
	if (FParse::Value(CommandLine, TEXT("fccity="), SeedString))
	{
		SpawnFromSeed(FCString::Strtoui64(*SeedString, nullptr, 10),
			FParse::Param(CommandLine, TEXT("fcrun")),
			FParse::Param(CommandLine, TEXT("fclistener")),
			FParse::Param(CommandLine, TEXT("fcrain")));
		return;
	}

	// THE DEFAULT BOOT PATH: double-clicking the exe with no flags starts a
	// run in a fresh city - the roguelike promise. Test scenes still own
	// their flags.
	const bool bAnyTestScene =
		FParse::Param(CommandLine, TEXT("fcdevscene"))
		|| FParse::Param(CommandLine, TEXT("fcaddress"))
		|| FParse::Value(CommandLine, TEXT("fcgenbuilding="), SeedString);
	if (bAnyTestScene)
	{
		return;
	}

	if (GAutoRunSeed == 0)
	{
		GAutoRunSeed = FPlatformTime::Cycles64() | 1ull; // gameplay-side RNG only - never generation-internal
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCCITY] AUTO-RUN seed %llu"), GAutoRunSeed);
	if (UFCDirectorSubsystem* Director = InWorld.GetSubsystem<UFCDirectorSubsystem>())
	{
		Director->EnableNow();
	}
	SpawnFromSeed(GAutoRunSeed, /*bRunLayer*/ true, /*bListener*/ true, /*bRain*/ false);
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

bool UFCCitySubsystem::SpawnFromSeed(const uint64 Seed, const bool bRunLayer,
	const bool bListener, const bool bRain)
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
				StreetLightComponents.Add(Component);
			}
		}
	}

	// --- The run layer (-fcrun): 2 conditions - a key deep in lot 0 and the
	// street substation. Streets START dark; restoring power is a condition,
	// which makes the whole district navigable and lethal at once (ROADMAP
	// 4.3 - the strongest expression of P4). ---
	if (bRunLayer)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (UFCRunSubsystem* Run = World->GetSubsystem<UFCRunSubsystem>())
		{
			Run->SetConditionsRequired(2);
		}
		// Key: top floor of lot 0.
		{
			const FFCCityLot& Lot = City.Lots[0];
			int32 KeyRoom = INDEX_NONE;
			int32 BestArea = 0;
			for (const FFCGenRoom& Room : Lot.Building.Rooms)
			{
				const int32 Area = (Room.CellMax.X - Room.CellMin.X) * (Room.CellMax.Y - Room.CellMin.Y);
				if (Room.Floor == Lot.Building.Floors - 1 && Area > BestArea)
				{
					KeyRoom = Room.Id;
					BestArea = Area;
				}
			}
			if (KeyRoom != INDEX_NONE)
			{
				const FFCGenRoom& Room = Lot.Building.Rooms[KeyRoom];
				World->SpawnActor<AFCKeyItem>(Lot.Origin + FVector(
					(Room.CellMin.X + Room.CellMax.X) * 0.5f * CellSize,
					(Room.CellMin.Y + Room.CellMax.Y) * 0.5f * CellSize,
					Room.Floor * FloorHeight + 60.0f), FRotator::ZeroRotator, Params);
			}
		}
		// Substation on the mid south street: dark streets until thrown.
		{
			const float MidX = (City.ExtentMin.X + City.ExtentMax.X) * 0.5f;
			if (AFCBreakerPanel* Substation = World->SpawnActor<AFCBreakerPanel>(
				FVector(MidX + 700, -StreetWidth * 0.5f, 120), FRotator::ZeroRotator, Params))
			{
				Substation->SetLabel(TEXT("Street power"));
				Substation->SetInitialOn(false);
				Substation->bSatisfiesConditionWhenOn = true;
				for (const TWeakObjectPtr<ULightComponent>& Light : StreetLightComponents)
				{
					Substation->LinkLight(Light.Get());
				}
			}
		}
		// Extraction pad at the south edge.
		{
			const float MidX = (City.ExtentMin.X + City.ExtentMax.X) * 0.5f;
			World->SpawnActor<AFCExtractZone>(FVector(MidX - 1400, -StreetWidth - 600, 10),
				FRotator::ZeroRotator, Params);
		}
	}

	// --- Shells for every lot (the skyline tier) ---
	SpawnedLots.SetNum(City.Lots.Num());
	for (int32 LotIndex = 0; LotIndex < City.Lots.Num(); ++LotIndex)
	{
		FC::Spawn::SpawnShell(World, City.Lots[LotIndex].Building,
			City.Lots[LotIndex].Origin, SpawnedLots[LotIndex]);
	}

	// --- Weather (-fcrain): rain raises the ambient noise floor (ROADMAP
	// 7.2) - cover for you, mobility for the Listener. Visual rain rides the
	// art pass; the SYSTEM ships now. ---
	if (bRain)
	{
		if (UFCNoiseSubsystem* Noise = World->GetSubsystem<UFCNoiseSubsystem>())
		{
			Noise->SetAmbientNoiseFloor(12.0f);
			UE_LOG(LogFootcandle, Display, TEXT("[FCCITY] rain: ambient noise floor 12"));
		}
	}
	// --- The Listener: the sound hunter on the mid street ---
	if (bListener)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		const float MidX = (City.ExtentMin.X + City.ExtentMax.X) * 0.5f;
		const float MidY = (City.ExtentMin.Y + City.ExtentMax.Y) * 0.5f;
		if (AFCListener* Listener = World->SpawnActor<AFCListener>(
			FVector(MidX + 2200, MidY, 120), FRotator(0, 180, 0), Params))
		{
			Listener->SetPatrolPoints({
				FVector(MidX + 2200, MidY, 120),
				FVector(MidX - 2200, MidY, 120),
			});
		}
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
