#include "Testing/FCM1SmokeSubsystem.h"

#include "Components/CapsuleComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Footcandle.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Player/FCPlayerCharacter.h"
#include "World/FCDoor.h"
#include "World/FCHideSpot.h"
#include "World/FCLightSwitch.h"

#if !UE_BUILD_SHIPPING

namespace
{
	template <typename T>
	T* FindNearest(UWorld* World, const FVector& Near)
	{
		T* Best = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<T> It(World); It; ++It)
		{
			const float DistSq = FVector::DistSquared(It->GetActorLocation(), Near);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = *It;
			}
		}
		return Best;
	}

	AFCPlayerCharacter* GetPlayer(UWorld* World)
	{
		const APlayerController* PC = World->GetFirstPlayerController();
		return PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	}
}

void UFCM1SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("fcm1smoke")))
	{
		return;
	}

	if (UFCNoiseSubsystem* Noise = InWorld.GetSubsystem<UFCNoiseSubsystem>())
	{
		NoiseHandle = Noise->OnNoiseEmitted.AddLambda([this](const FFCNoiseEvent& Event)
		{
			++NoiseEventCount;
			if (Event.SourceTag == TEXT("Noise.Source.Footstep"))
			{
				++FootstepNoiseCount;
			}
		});
	}

	UE_LOG(LogFootcandle, Display, TEXT("[FCM1SMOKE] starting"));
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCM1SmokeSubsystem::Tick));
}

void UFCM1SmokeSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	Super::Deinitialize();
}

void UFCM1SmokeSubsystem::Check(const TCHAR* Name, const bool bCondition)
{
	if (bCondition)
	{
		++PassCount;
		UE_LOG(LogFootcandle, Display, TEXT("[FCM1SMOKE] PASS %s"), Name);
	}
	else
	{
		++FailCount;
		UE_LOG(LogFootcandle, Error, TEXT("[FCM1SMOKE] FAIL %s"), Name);
	}
}

bool UFCM1SmokeSubsystem::Tick(float /*DeltaTime*/)
{
	UWorld* World = GetWorld();
	AFCPlayerCharacter* Player = GetPlayer(World);
	if (Player == nullptr)
	{
		return true; // wait for possession
	}
	++Frame;

	const bool bGrounded = Player->GetCharacterMovement() != nullptr
		&& Player->GetCharacterMovement()->IsMovingOnGround();

	switch (Step)
	{
	case 0: // settle AND be on the ground before driving movement
		if (Frame >= 30 && bGrounded)
		{
			MoveStart = Player->GetActorLocation();
			Frame = 30; // normalize so later phase windows line up
			Step = 1;
		}
		break;

	case 1: // walk ~3s of frames; footsteps should fire (stride 170cm)
		Player->AddMovementInput(FVector(0, 1, 0), 1.0f);
		if (Frame >= 210)
		{
			UE_LOG(LogFootcandle, Display,
				TEXT("[FCM1SMOKE] walk debug: start=%s now=%s vel=%s grounded=%d mode=%d maxspeed=%.0f"),
				*MoveStart.ToCompactString(), *Player->GetActorLocation().ToCompactString(),
				*Player->GetVelocity().ToCompactString(), bGrounded ? 1 : 0,
				static_cast<int32>(Player->GetCharacterMovement()->MovementMode.GetValue()),
				Player->GetCharacterMovement()->MaxWalkSpeed);
			Check(TEXT("Move: walked >100cm"),
				FVector::Dist2D(Player->GetActorLocation(), MoveStart) > 100.0f);
			Check(TEXT("Footsteps: noise emitted while walking"), FootstepNoiseCount > 0);
			StaminaBefore = Player->GetStamina();
			Player->TestSetSprint(true);
			Step = 2;
		}
		break;

	case 2: // sprint 60 frames; stamina must drain
		Player->AddMovementInput(FVector(0, 1, 0), 1.0f);
		if (Frame >= 270)
		{
			Player->TestSetSprint(false);
			Check(TEXT("Stamina: drained while sprinting"),
				Player->GetStamina() < StaminaBefore - 1.0f);
			// Entry door: interact quietly.
			if (AFCDoor* Door = FindNearest<AFCDoor>(World, FVector(450, 0, 0)))
			{
				const int32 Before = NoiseEventCount;
				Door->Interact(Player, /*bQuiet*/ true);
				Check(TEXT("Door: interact emitted noise"), NoiseEventCount > Before);
			}
			else
			{
				Check(TEXT("Door: found"), false);
			}
			Step = 3;
		}
		break;

	case 3: // give the quiet swing time, then verify it opened
		if (Frame >= 470)
		{
			const AFCDoor* Door = FindNearest<AFCDoor>(World, FVector(450, 0, 0));
			Check(TEXT("Door: opened after quiet swing"), Door != nullptr && Door->IsOpen());

			// Light switch: toggle and verify the west-room light went dark.
			AFCLightSwitch* Switch = FindNearest<AFCLightSwitch>(World, FVector(430, 30, 120));
			const APointLight* WestLight = FindNearest<APointLight>(World, FVector(300, 300, 270));
			if (Switch != nullptr && WestLight != nullptr)
			{
				Switch->Interact(Player, false);
				Check(TEXT("Switch: light toggled off"),
					!WestLight->GetLightComponent()->IsVisible());
				Switch->Interact(Player, false);
				Check(TEXT("Switch: light toggled back on"),
					WestLight->GetLightComponent()->IsVisible());
			}
			else
			{
				Check(TEXT("Switch: found"), Switch != nullptr && WestLight != nullptr);
			}

			// Hide spot: teleport upstairs next to the locker and enter.
			if (AFCHideSpot* Hide = FindNearest<AFCHideSpot>(World, FVector(80, 80, 440)))
			{
				Player->TeleportTo(Hide->GetActorLocation() + FVector(150, 150, 40),
					FRotator::ZeroRotator, false, true);
				HideEntryFrom = Player->GetActorLocation();
				Hide->Interact(Player, false);
				Check(TEXT("Hide: entered (moved to spot)"),
					FVector::Dist(Player->GetActorLocation(), Hide->GetActorLocation()) < 250.0f);
				Hide->Interact(Player, false);
				Check(TEXT("Hide: exited near entry point"),
					FVector::Dist(Player->GetActorLocation(), HideEntryFrom) < 120.0f);
			}
			else
			{
				Check(TEXT("Hide: found"), false);
			}

			// Vault: back to the street, spawn an 80cm crate, vault it.
			Player->TeleportTo(FVector(200, -400, 120), FRotator(0, 90, 0), false, true);
			Step = 4;
		}
		break;

	case 4: // wait until grounded after the street teleport, then vault
		if (Frame >= 480 && bGrounded)
		{
			// 80 cm obstacle directly north of the player.
			const FVector Min(160, -320, 0);
			const FVector Max(240, -240, 80);
			// SpawnBox lives in the address subsystem; inline a crate here.
			if (UWorld* W = GetWorld())
			{
				FActorSpawnParameters Params;
				Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AStaticMeshActor* Crate = W->SpawnActor<AStaticMeshActor>(
					(Min + Max) * 0.5f, FRotator::ZeroRotator, Params))
				{
					if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
					{
						Crate->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
						Crate->GetStaticMeshComponent()->SetStaticMesh(Cube);
						Crate->SetActorScale3D((Max - Min) / 100.0f);
					}
				}
			}
			Check(TEXT("Vault: started against 80cm crate"), Player->TestVault());
			Frame = 480;
			Step = 5;
		}
		break;

	case 5: // vault completes in ~0.35s; then M3 propagation checks
		if (Frame >= 560)
		{
			Check(TEXT("Vault: landed on/past the crate"),
				Player->GetActorLocation().Y > -330.0f
				&& Player->GetActorLocation().Z > 100.0f);

			// M3: the portal graph is live in-world. A noise in the west room
			// must reach the east room MUCH louder once the interior door
			// opens (delta == aperture table, 22 - 5 = 17).
			if (UFCNoiseSubsystem* Noise = World->GetSubsystem<UFCNoiseSubsystem>())
			{
				FFCNoiseEvent Probe;
				Probe.Origin = FVector(300, 300, 150);   // west room
				Probe.Loudness = 60.0f;
				const FVector EastEar(800, 200, 150);    // east/stair room
				const FVector StreetEar(500, -300, 150); // outside, door closed

				const float EastClosed = Noise->PerceivedLoudnessAt(Probe, EastEar);
				const float StreetClosed = Noise->PerceivedLoudnessAt(Probe, StreetEar);

				// Open the interior door instantly (test path: snap the portal).
				if (AFCDoor* Interior = FindNearest<AFCDoor>(World, FVector(610, 150, 0)))
				{
					Interior->Interact(Player, /*bQuiet*/ false);
				}
				Frame = 560;
				Step = 6;
				// Stash for the comparison after the swing.
				MoveStart = FVector(EastClosed, StreetClosed, 0);
			}
			else
			{
				Finish();
				return false;
			}
		}
		break;

	case 6: // interior door has swung open; re-measure propagation
		if (Frame >= 640)
		{
			if (UFCNoiseSubsystem* Noise = World->GetSubsystem<UFCNoiseSubsystem>())
			{
				FFCNoiseEvent Probe;
				Probe.Origin = FVector(300, 300, 150);
				Probe.Loudness = 60.0f;
				const float EastOpen = Noise->PerceivedLoudnessAt(Probe, FVector(800, 200, 150));
				const float EastClosed = MoveStart.X;
				const float StreetClosed = MoveStart.Y;

				Check(TEXT("M3: east room louder through open door (+~17)"),
					EastOpen > EastClosed + 14.0f && EastOpen < EastClosed + 20.0f);
				// The west room has an OPEN (glassless) window: the street
				// hears loudly through it no matter what the door does -
				// "open windows leak" is a design truth, verified here.
				Check(TEXT("M3: open window leaks loudly to the street"),
					StreetClosed > 40.0f);
				// Multi-hop: west -> (open door) -> east -> stairwell -> upper.
				const float Upper = Noise->PerceivedLoudnessAt(Probe, FVector(500, 300, 480));
				Check(TEXT("M3: upper floor hears via multi-hop path"),
					Upper > 20.0f && Upper < Probe.Loudness);
			}
			// Crouch-height regression (playtest bug: eye went under the
			// floor): stand on the street, crouch, measure the camera.
			Player->TeleportTo(FVector(500, -600, 120), FRotator(0, 90, 0), false, true);
			Frame = 640;
			Step = 7;
		}
		break;

	case 7:
		if (Frame >= 700 && bGrounded)
		{
			const float TestFloorZ = Player->GetActorLocation().Z
				- Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			Player->TestToggleCrouch();
			Frame = 700;
			Step = 8;
			MoveStart = FVector(TestFloorZ, 0, 0); // stash floor height
		}
		break;

	case 8: // crouched eye must sit near 105cm over the floor - never sunken
		if (Frame >= 760)
		{
			const float TestFloorZ = MoveStart.X;
			const float EyeAboveFloor = Player->TestGetCameraLocation().Z - TestFloorZ;
			UE_LOG(LogFootcandle, Display, TEXT("[FCM1SMOKE] crouched eye %.0fcm above floor"), EyeAboveFloor);
			Check(TEXT("Crouch: eye ~105cm over the floor (not sunken)"),
				EyeAboveFloor > 85.0f && EyeAboveFloor < 125.0f);
			Player->TestToggleCrouch(); // stand back up

			// Window climb-through (playtest ask): outside the west window
			// (sill 90, y 250..350 at x=0), facing +X into the building.
			// Close enough that the reach (90cm from center) spans the wall.
			// Controller rotation wins over pawn rotation - set BOTH.
			Player->TeleportTo(FVector(-90, 300, 120), FRotator(0, 0, 0), false, true);
			if (AController* Controller = Player->GetController())
			{
				Controller->SetControlRotation(FRotator(0, 0, 0));
			}
			Frame = 760;
			Step = 9;
		}
		break;

	case 9:
		if (Frame >= 820 && bGrounded)
		{
			Check(TEXT("Window: climb-through starts at the sill"), Player->TestVault());
			Frame = 820;
			Step = 10;
		}
		break;

	case 10: // arc completes; the player must be INSIDE the west room
		if (Frame >= 900)
		{
			Check(TEXT("Window: landed inside through the opening"),
				Player->GetActorLocation().X > 20.0f
				&& FMath::Abs(Player->GetActorLocation().Y - 300.0f) < 200.0f);
			Finish();
			return false;
		}
		break;

	default:
		break;
	}
	return true;
}

void UFCM1SmokeSubsystem::Finish()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCM1SMOKE] DONE pass=%d fail=%d noise_events=%d"),
		PassCount, FailCount, NoiseEventCount);
	if (UFCNoiseSubsystem* Noise = GetWorld()->GetSubsystem<UFCNoiseSubsystem>())
	{
		Noise->OnNoiseEmitted.Remove(NoiseHandle);
	}
	FPlatformMisc::RequestExit(false);
}

#else // UE_BUILD_SHIPPING

void UFCM1SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UFCM1SmokeSubsystem::Deinitialize() { Super::Deinitialize(); }
bool UFCM1SmokeSubsystem::Tick(float) { return false; }
void UFCM1SmokeSubsystem::Check(const TCHAR*, bool) {}
void UFCM1SmokeSubsystem::Finish() {}

#endif
