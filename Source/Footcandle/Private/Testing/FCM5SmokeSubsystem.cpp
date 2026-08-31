#include "Testing/FCM5SmokeSubsystem.h"

#include "Engine/World.h"
#include "Footcandle.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Noise/FCNoiseSubsystem.h"
#include "Player/FCPlayerCharacter.h"
#include "Save/FCSaveSubsystem.h"
#include "Testing/FCGenBuildingSubsystem.h"
#include "World/FCDoor.h"

#if !UE_BUILD_SHIPPING

void UFCM5SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("fcm5smoke")))
	{
		return;
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCM5SMOKE] starting"));
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCM5SmokeSubsystem::Tick));
}

void UFCM5SmokeSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	Super::Deinitialize();
}

void UFCM5SmokeSubsystem::Check(const TCHAR* Name, const bool bCondition)
{
	if (bCondition)
	{
		++PassCount;
		UE_LOG(LogFootcandle, Display, TEXT("[FCM5SMOKE] PASS %s"), Name);
	}
	else
	{
		++FailCount;
		UE_LOG(LogFootcandle, Error, TEXT("[FCM5SMOKE] FAIL %s"), Name);
	}
}

bool UFCM5SmokeSubsystem::Tick(const float DeltaTime)
{
	using namespace FC::Gen;
	UWorld* World = GetWorld();
	UFCGenBuildingSubsystem* Gen = World->GetSubsystem<UFCGenBuildingSubsystem>();
	const APlayerController* PC = World->GetFirstPlayerController();
	AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	if (Gen == nullptr || Player == nullptr)
	{
		return true;
	}
	StepTime += DeltaTime;
	const FFCBuildingData& Building = Gen->GetBuildingData();

	switch (Step)
	{
	case 0: // spawned and structurally bound
		if (StepTime >= 2.0f)
		{
			int32 DoorPortals = 0;
			for (const FFCGenPortal& Portal : Building.Portals)
			{
				if (Portal.Kind == EGenPortalKind::ExteriorDoor
					|| Portal.Kind == EGenPortalKind::InteriorDoor)
				{
					++DoorPortals;
				}
			}
			Check(TEXT("Gen: building spawned with rooms"), Building.Rooms.Num() > 0);
			Check(TEXT("Gen: one door actor per door portal"),
				Gen->GetDoors().Num() == DoorPortals && DoorPortals > 0);

			// Propagation through GENERATED topology: noise in the entry room
			// heard quieter on the street than in the room (closed doors).
			if (UFCNoiseSubsystem* Noise = World->GetSubsystem<UFCNoiseSubsystem>())
			{
				const FFCGenRoom& Room = Building.Rooms[0];
				FFCNoiseEvent Probe;
				Probe.Origin = FVector(
					(Room.CellMin.X + Room.CellMax.X) * 0.5f * CellSize,
					(Room.CellMin.Y + Room.CellMax.Y) * 0.5f * CellSize,
					Room.Floor * FloorHeight + 120.0f);
				Probe.Loudness = 60.0f;
				const float Inside = Noise->PerceivedLoudnessAt(Probe, Probe.Origin + FVector(50, 50, 0));
				const float Street = Noise->PerceivedLoudnessAt(Probe,
					FVector(Building.FootprintCells.X * CellSize * 0.5f, -1500, 120));
				// Open windows LEAK by design (aperture loss 6) - so the
				// honest assertion is "attenuated by at least the cheapest
				// aperture + distance", not "muffled like a bunker".
				UE_LOG(LogFootcandle, Display,
					TEXT("[FCM5SMOKE] propagation: inside=%.1f street=%.1f"), Inside, Street);
				Check(TEXT("Gen: generated topology attenuates to the street"),
					Inside > Street + 4.0f && Street < Probe.Loudness);
			}
			Step = 1;
			StepTime = 0.0f;
		}
		break;

	case 1: // save v1 round trip through the delta log
		{
			UFCSaveSubsystem* Save = World->GetGameInstance()->GetSubsystem<UFCSaveSubsystem>();
			if (Save == nullptr || Gen->GetDoors().Num() == 0)
			{
				Check(TEXT("Save: subsystem/doors present"), false);
				Finish();
				return false;
			}
			AFCDoor* Door = Gen->GetDoors()[0];
			Door->SetOpenInstant(true);
			SavedSpot = Player->GetActorLocation();
			Check(TEXT("Save: wrote slot"), Save->SaveNow(TEXT("FCSmoke")));

			// Diverge: close the door, move the player away.
			Door->SetOpenInstant(false);
			Player->TeleportTo(SavedSpot + FVector(600, -600, 0), FRotator::ZeroRotator, false, true);

			Check(TEXT("Save: loaded slot"), Save->LoadNow(TEXT("FCSmoke")));
			Check(TEXT("Save: door state restored (open)"), Door->IsOpen());
			Check(TEXT("Save: player position restored"),
				FVector::Dist2D(Player->GetActorLocation(), SavedSpot) < 80.0f);
			Finish();
			return false;
		}

	default:
		break;
	}
	return true;
}

void UFCM5SmokeSubsystem::Finish()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCM5SMOKE] DONE pass=%d fail=%d"), PassCount, FailCount);
	FPlatformMisc::RequestExit(false);
}

#else

void UFCM5SmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UFCM5SmokeSubsystem::Deinitialize() { Super::Deinitialize(); }
bool UFCM5SmokeSubsystem::Tick(float) { return false; }
void UFCM5SmokeSubsystem::Check(const TCHAR*, bool) {}
void UFCM5SmokeSubsystem::Finish() {}

#endif
