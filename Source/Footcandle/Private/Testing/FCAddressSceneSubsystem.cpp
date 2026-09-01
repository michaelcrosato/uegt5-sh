#include "Testing/FCAddressSceneSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "World/FCLightFixture.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "AI/FCWatcher.h"
#include "Misc/CommandLine.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Perception/FCLightRegistry.h"
#include "Testing/FCTestStationSubsystem.h"
#include "World/FCDoor.h"
#include "World/FCHideSpot.h"
#include "World/FCLightSwitch.h"
#include "World/FCNoiseProp.h"

namespace
{
	FAutoConsoleCommandWithWorld GFCAddressCmd(
		TEXT("fc.Address.Spawn"),
		TEXT("Spawn the two-floor 'One Address' test building."),
		FConsoleCommandWithWorldDelegate::CreateLambda(
			[](UWorld* World)
			{
				if (World != nullptr)
				{
					if (UFCAddressSceneSubsystem* Subsystem = World->GetSubsystem<UFCAddressSceneSubsystem>())
					{
						Subsystem->SpawnScene();
					}
				}
			}));

	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

	// Building footprint (cm): X 0..1000 (east), Y 0..600 (north).
	constexpr float FloorZ = 320.0f;   // floor-to-floor (grid, ROADMAP 5.3)
	constexpr float WallT = 20.0f;
}

void UFCAddressSceneSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld())
	{
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("fcaddress")))
	{
		SpawnScene();
	}
}

AStaticMeshActor* UFCAddressSceneSubsystem::SpawnBox(const FVector& MinCorner, const FVector& MaxCorner)
{
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, CubePath);
	if (Cube == nullptr)
	{
		return nullptr;
	}
	const FVector Center = (MinCorner + MaxCorner) * 0.5f;
	const FVector Size = MaxCorner - MinCorner;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Center, FRotator::ZeroRotator, Params);
	if (Actor != nullptr)
	{
		Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
		Actor->SetActorScale3D(Size / 100.0f);
	}
	return Actor;
}

void UFCAddressSceneSubsystem::BuildWall(const bool bAlongX, const float FixedAxisPos,
	const float AxisStart, const float AxisEnd, const float Z0, const float Z1,
	const float Thickness, const TArray<FOpening>& Openings)
{
	// Sort openings, then emit: solid slices between them + lintels/sills.
	TArray<FOpening> Sorted = Openings;
	Sorted.Sort([](const FOpening& A, const FOpening& B) { return A.Start < B.Start; });

	auto Emit = [&](const float A0, const float A1, const float BZ, const float TZ)
	{
		if (A1 - A0 < 1.0f || TZ - BZ < 1.0f)
		{
			return;
		}
		if (bAlongX)
		{
			SpawnBox(FVector(A0, FixedAxisPos, BZ), FVector(A1, FixedAxisPos + Thickness, TZ));
		}
		else
		{
			SpawnBox(FVector(FixedAxisPos, A0, BZ), FVector(FixedAxisPos + Thickness, A1, TZ));
		}
	};

	float Cursor = AxisStart;
	for (const FOpening& Opening : Sorted)
	{
		Emit(Cursor, Opening.Start, Z0, Z1);             // solid before opening
		Emit(Opening.Start, Opening.End, Z0, Opening.BottomZ); // sill below
		Emit(Opening.Start, Opening.End, Opening.TopZ, Z1);    // lintel above
		Cursor = Opening.End;
	}
	Emit(Cursor, AxisEnd, Z0, Z1);
}

void UFCAddressSceneSubsystem::SpawnScene()
{
	if (bSpawned)
	{
		return;
	}
	bSpawned = true;
	UWorld* World = GetWorld();

	// --- Slabs ---
	SpawnBox(FVector(-2000, -2000, -20), FVector(3000, 2600, 0));       // street/ground
	// Second floor with a 400x200 stair opening at the NE corner.
	SpawnBox(FVector(0, 0, FloorZ), FVector(1000, 400, FloorZ + 20));
	SpawnBox(FVector(0, 400, FloorZ), FVector(600, 600, FloorZ + 20));
	SpawnBox(FVector(0, 0, 2 * FloorZ), FVector(1000, 600, 2 * FloorZ + 20)); // roof

	// --- Ground-floor perimeter ---
	// X-running walls span -WallT..1000+WallT so the corners are sealed -
	// a 20 cm corner slit leaks light, and light leaks are gameplay bugs
	// (LGT-08; caught by eye in the first FC_AddrStreet capture).
	// South wall (y=0): entry door 450..550 (100 x 210 per grid).
	BuildWall(true, -WallT, -WallT, 1000 + WallT, 0, FloorZ, WallT,
		{ { 450, 550, 0, 210 } });
	// North wall (y=600).
	BuildWall(true, 600, -WallT, 1000 + WallT, 0, FloorZ, WallT, {});
	// West wall (x=0): window y 250..350, sill 90..220.
	BuildWall(false, -WallT, 0, 600, 0, FloorZ, WallT,
		{ { 250, 350, 90, 220 } });
	// East wall (x=1000).
	BuildWall(false, 1000, 0, 600, 0, FloorZ, WallT, {});

	// --- Upper-floor perimeter ---
	// South: window 300..450, sill at FloorZ+90.
	BuildWall(true, -WallT, -WallT, 1000 + WallT, FloorZ, 2 * FloorZ, WallT,
		{ { 300, 450, FloorZ + 90, FloorZ + 220 } });
	BuildWall(true, 600, -WallT, 1000 + WallT, FloorZ, 2 * FloorZ, WallT, {});
	BuildWall(false, -WallT, 0, 600, FloorZ, 2 * FloorZ, WallT, {});
	BuildWall(false, 1000, 0, 600, FloorZ, 2 * FloorZ, WallT, {});

	// --- Interior partition (x=600, ground floor) with door at y 150..250 ---
	BuildWall(false, 600, 0, 600, 0, FloorZ, WallT,
		{ { 150, 250, 0, 210 } });

	// --- Stairs: blocky straight run up the east room (rise 32, run 40) ---
	for (int32 Step = 0; Step < 10; ++Step)
	{
		const float X0 = 620.0f + Step * 38.0f;
		SpawnBox(FVector(X0, 420, 0), FVector(X0 + 38.0f, 600, 32.0f * (Step + 1)));
	}

	// --- Acoustic room graph (ROADMAP 7.2): interior rooms first so point
	// resolution prefers them over the enclosing street cell. ---
	int32 EntryPortal = INDEX_NONE;
	int32 InteriorPortal = INDEX_NONE;
	if (UFCNoiseSubsystem* Noise = World->GetSubsystem<UFCNoiseSubsystem>())
	{
		using namespace FC::Gen;
		FRoomGraph& Graph = Noise->GetRoomGraph();
		const int32 WestRoom = Graph.AddRoom(FVector(0, 0, 0), FVector(600, 600, 320));
		const int32 EastRoom = Graph.AddRoom(FVector(600, 0, 0), FVector(1000, 600, 320));
		const int32 UpperRoom = Graph.AddRoom(FVector(0, 0, 320), FVector(1000, 600, 640));
		const int32 Street = Graph.AddRoom(FVector(-2000, -2000, 0), FVector(3000, 2600, 640));

		EntryPortal = Graph.AddPortal(Street, WestRoom, FVector(500, 0, 105), EAperture::DoorClosedExterior);
		InteriorPortal = Graph.AddPortal(WestRoom, EastRoom, FVector(600, 200, 105), EAperture::DoorClosedInterior);
		Graph.AddPortal(WestRoom, Street, FVector(0, 300, 155), EAperture::WindowOpen);   // west window (no glass)
		Graph.AddPortal(EastRoom, UpperRoom, FVector(800, 500, 320), EAperture::Stairwell);
		Graph.AddPortal(UpperRoom, Street, FVector(375, 0, 475), EAperture::WindowOpen);  // upper window
	}

	// --- Doors (bound to their acoustic portals) ---
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// Entry door: hinge at west jamb of the south opening.
		if (AFCDoor* Entry = World->SpawnActor<AFCDoor>(FVector(450, -WallT * 0.5f, 0), FRotator::ZeroRotator, Params))
		{
			Entry->BindAcousticPortal(EntryPortal, /*bExterior*/ true);
		}
		// Interior door: hinge at south jamb, leaf along Y.
		if (AFCDoor* Interior = World->SpawnActor<AFCDoor>(FVector(600 + WallT * 0.5f, 150, 0), FRotator(0, 90, 0), Params))
		{
			Interior->BindAcousticPortal(InteriorPortal, /*bExterior*/ false);
		}
	}

	// --- Lights (movable, shadow-casting; per-room practicals) ---
	// Every gameplay light is now a visible FIXTURE (decision #29): a body
	// you can see, flip with F, and shatter with a thrown prop. Configure()
	// registers each with the perception registry: "looks dark" and "is
	// dark" stay the same model (ROADMAP 8.3).
	ULightComponent* WestRoomLight = nullptr;
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		// West room bulb: LOW-WATTAGE, warm amber, in the NW corner with a
		// gentle mains hum (survival-horror relight, decision #30). Corner
		// placement rakes shadows the length of the room; 60cd from the
		// center flattened it into a showroom.
		if (AFCLightFixture* Fixture = World->SpawnActor<AFCLightFixture>(
			FVector(120, 480, 276), FRotator::ZeroRotator, Params))
		{
			Fixture->Configure(EFCFixtureStyle::CeilingBulb,
				FLinearColor(1.0f, 0.72f, 0.45f), 16.0f, 1100.0f,
				EFCFlickerStyle::MainsHum, /*bWithFlicker*/ true, /*Seed*/ 11ull);
			WestRoomLight = Fixture->GetLightComponent();
		}
		// Stair-room emergency LED: dim, red, high - and failing (the
		// flicker component's regression fixture, ROADMAP 6.2).
		if (AFCLightFixture* Fixture = World->SpawnActor<AFCLightFixture>(
			FVector(820, 200, 284), FRotator::ZeroRotator, Params))
		{
			Fixture->Configure(EFCFixtureStyle::EmergencyLED,
				FLinearColor(1.0f, 0.22f, 0.15f), 18.0f, 900.0f,
				EFCFlickerStyle::FailingTube, /*bWithFlicker*/ true, /*Seed*/ 2ull);
		}
		// Upper room bulb: dim, sickly green, far corner - a different
		// temperature per room so the palette reads (decision #30).
		if (AFCLightFixture* Fixture = World->SpawnActor<AFCLightFixture>(
			FVector(720, 160, FloorZ + 266), FRotator::ZeroRotator, Params))
		{
			Fixture->Configure(EFCFixtureStyle::CeilingBulb,
				FLinearColor(0.68f, 0.82f, 0.62f), 10.0f, 1000.0f);
		}
		// Sodium streetlight on a real pole, head at the old light position.
		if (AFCLightFixture* Fixture = World->SpawnActor<AFCLightFixture>(
			FVector(500, -650, 550), FRotator::ZeroRotator, Params))
		{
			Fixture->Configure(EFCFixtureStyle::Streetlight,
				FLinearColor(1.0f, 0.64f, 0.23f), 800.0f, 2200.0f);
			// Capture-tuned aim, kept verbatim: straight down at full power
			// washed the facade and killed the doorway shadow - this grazes
			// the wall and pools on the street instead ([FCLUX] history).
			if (USpotLightComponent* Spot = Cast<USpotLightComponent>(Fixture->GetLightComponent()))
			{
				Spot->SetWorldRotation(FRotator(-55.0f, 90.0f, 0.0f));
				Spot->SetInnerConeAngle(20.0f);
				Spot->SetOuterConeAngle(32.0f);
				Spot->SetVolumetricScatteringIntensity(2.5f);
			}
		}
	}

	// --- Moon: weak cool directional on VSM (MegaLights excludes
	// directionals; VSM is its documented best case). Weak on purpose -
	// practicals must matter (ROADMAP 6.2). ---
	{
		ADirectionalLight* Moon = World->SpawnActor<ADirectionalLight>(
			FVector(0, 0, 2000), FRotator(-35.0f, 40.0f, 0.0f));
		if (Moon != nullptr)
		{
			Moon->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			UDirectionalLightComponent* Component =
				CastChecked<UDirectionalLightComponent>(Moon->GetLightComponent());
			Component->SetIntensity(0.08f); // lux - deep night; capture-tuned from 0.4 which read as dusk
			Component->SetLightColor(FLinearColor(0.55f, 0.65f, 0.95f));
			if (UFCLightRegistry* Registry = World->GetSubsystem<UFCLightRegistry>())
			{
				Registry->RegisterLight(Component);
			}
		}
	}

	// --- Night exposure: cap auto-exposure so darkness STAYS dark instead
	// of auto-brightening into grey mush (ROADMAP 6.9). Tuned by capture. ---
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APostProcessVolume* PP = World->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (PP != nullptr)
		{
			PP->bUnbound = true;
			PP->Settings.bOverride_AutoExposureMinBrightness = true;
			PP->Settings.AutoExposureMinBrightness = 0.02f;
			PP->Settings.bOverride_AutoExposureMaxBrightness = true;
			// Max raised from 0.18 (flash-check audit): the low cap nuked the
			// flashlight disc to clipped white; darkness is guarded by MIN.
			PP->Settings.AutoExposureMaxBrightness = 1.5f;
			PP->Settings.bOverride_AutoExposureBias = true;
			PP->Settings.AutoExposureBias = -0.4f;
		}
	}

	// --- Light switch in the west room, wired to its practical ---
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AFCLightSwitch* Switch = World->SpawnActor<AFCLightSwitch>(
			FVector(430, 30, 120), FRotator::ZeroRotator, Params);
		if (Switch != nullptr && WestRoomLight != nullptr)
		{
			Switch->LinkLight(WestRoomLight);
		}
	}

	// --- Noise props + a throwable-scale bottle stand-in ---
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector PropSpots[] = { {200, 150, 40}, {260, 480, 40}, {520, 420, 40} };
		for (const FVector& Spot : PropSpots)
		{
			if (AFCNoiseProp* Prop = World->SpawnActor<AFCNoiseProp>(Spot, FRotator::ZeroRotator, Params))
			{
				Prop->ConfigureMesh(CubePath, FVector(0.28f), 45.0f);
			}
		}
		if (AFCNoiseProp* Bottle = World->SpawnActor<AFCNoiseProp>(FVector(340, 90, 40), FRotator::ZeroRotator, Params))
		{
			Bottle->ConfigureMesh(CylinderPath, FVector(0.10f, 0.10f, 0.30f), 55.0f);
		}
	}

	// --- Hide-in locker, upper floor SW corner ---
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AFCHideSpot>(FVector(80, 80, FloorZ + 120), FRotator(0, -45, 0), Params);
	}

	// --- Stations (the M1 visual regression corpus) ---
	if (UFCTestStationSubsystem* Stations = World->GetSubsystem<UFCTestStationSubsystem>())
	{
		Stations->RegisterStation(TEXT("AddrStreet"), FVector(500, -900, 260), FRotator(-8.0f, 90.0f, 0.0f));
		Stations->RegisterStation(TEXT("AddrDoorway"), FVector(500, -220, 165), FRotator(-2.0f, 90.0f, 0.0f));
		Stations->RegisterStation(TEXT("AddrWestRoom"), FVector(320, 90, 165), FRotator(4.0f, 100.0f, 0.0f));
		Stations->RegisterStation(TEXT("AddrStairs"), FVector(660, 140, 165), FRotator(8.0f, 65.0f, 0.0f));
		Stations->RegisterStation(TEXT("AddrUpper"), FVector(160, 480, FloorZ + 165), FRotator(-6.0f, -35.0f, 0.0f));
		Stations->RegisterStation(TEXT("AddrWindowSpill"), FVector(-420, 300, 140), FRotator(4.0f, 0.0f, 0.0f));
	}

	// --- The Watcher (-fcwatcher): patrols the street, hunts light ---
	if (FParse::Param(FCommandLine::Get(), TEXT("fcwatcher")))
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AFCWatcher* Watcher = World->SpawnActor<AFCWatcher>(
			FVector(2000, -1000, 140), FRotator(0, 180, 0), Params))
		{
			Watcher->SetPatrolPoints({
				FVector(2000, -1000, 140),
				FVector(-1200, -1000, 140),
				FVector(500, -1600, 140),
			});
		}
	}

	// Put the player on the street facing the door.
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->TeleportTo(FVector(500, -400, 120), FRotator(0, 90, 0), false, true);
			PC->SetControlRotation(FRotator(0, 90, 0));
		}
	}

	UE_LOG(LogFootcandle, Display,
		TEXT("[FCADDR] One Address spawned: 2 floors, 2 doors, 4 light fixtures, switch, 4 props, locker, 6 stations"));
}
