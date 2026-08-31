#include "Save/FCSaveSubsystem.h"

#include "Engine/World.h"
#include "Footcandle.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/FCPlayerCharacter.h"
#include "Testing/FCGenBuildingSubsystem.h"
#include "World/FCDoor.h"

namespace
{
	UFCSaveSubsystem* GetSaveSubsystem()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		const UWorld* World = GEngine->GetCurrentPlayWorld();
		return World != nullptr && World->GetGameInstance() != nullptr
			? World->GetGameInstance()->GetSubsystem<UFCSaveSubsystem>()
			: nullptr;
	}

	FAutoConsoleCommand GFCSaveCmd(
		TEXT("fc.Save"),
		TEXT("Save the run to slot FCDev (seed + delta log)."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UFCSaveSubsystem* Subsystem = GetSaveSubsystem())
			{
				Subsystem->SaveNow(TEXT("FCDev"));
			}
		}));

	FAutoConsoleCommand GFCLoadCmd(
		TEXT("fc.Load"),
		TEXT("Restore the run from slot FCDev."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			if (UFCSaveSubsystem* Subsystem = GetSaveSubsystem())
			{
				Subsystem->LoadNow(TEXT("FCDev"));
			}
		}));
}

bool UFCSaveSubsystem::SaveNow(const FString& Slot)
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World->GetFirstPlayerController();
	const AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	const UFCGenBuildingSubsystem* Gen = World->GetSubsystem<UFCGenBuildingSubsystem>();
	if (Player == nullptr)
	{
		UE_LOG(LogFootcandle, Warning, TEXT("[FCSAVE] no player to save"));
		return false;
	}

	UFCSaveGame* Save = NewObject<UFCSaveGame>(GetTransientPackage());
	Save->Seed = Gen != nullptr ? Gen->GetBuildingData().Seed : 0;
	Save->PlayerLocation = Player->GetActorLocation();
	Save->PlayerRotation = PC->GetControlRotation();
	Save->Battery = Player->GetBattery();
	Save->Stamina = Player->GetStamina();
	if (Gen != nullptr)
	{
		for (const TObjectPtr<AFCDoor>& Door : Gen->GetDoors())
		{
			Save->DoorOpen.Add(Door != nullptr && Door->IsOpen());
		}
	}

	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
	UE_LOG(LogFootcandle, Display, TEXT("[FCSAVE] %s seed=%llu doors=%d -> %s"),
		bOk ? TEXT("saved") : TEXT("SAVE FAILED"), Save->Seed, Save->DoorOpen.Num(), *Slot);
	return bOk;
}

bool UFCSaveSubsystem::LoadNow(const FString& Slot)
{
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	const UFCGenBuildingSubsystem* Gen = World->GetSubsystem<UFCGenBuildingSubsystem>();

	UFCSaveGame* Save = Cast<UFCSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	if (Save == nullptr || Player == nullptr)
	{
		UE_LOG(LogFootcandle, Warning, TEXT("[FCSAVE] load failed (slot missing or no player)"));
		return false;
	}
	if (Gen != nullptr && Gen->GetBuildingData().Seed != Save->Seed)
	{
		// Honesty rule (ROADMAP 12): never silently regenerate a different
		// city under a save. Full travel-to-seed flow lands with the run
		// loop (M8); until then a mismatch refuses.
		UE_LOG(LogFootcandle, Error, TEXT("[FCSAVE] seed mismatch: world=%llu save=%llu - refusing"),
			Gen->GetBuildingData().Seed, Save->Seed);
		return false;
	}

	Player->TeleportTo(Save->PlayerLocation, FRotator(0, Save->PlayerRotation.Yaw, 0), false, true);
	PC->SetControlRotation(Save->PlayerRotation);
	Player->RestoreFromSave(Save->Battery, Save->Stamina);
	if (Gen != nullptr)
	{
		const TArray<TObjectPtr<AFCDoor>>& Doors = Gen->GetDoors();
		for (int32 Index = 0; Index < Doors.Num() && Index < Save->DoorOpen.Num(); ++Index)
		{
			if (Doors[Index] != nullptr)
			{
				Doors[Index]->SetOpenInstant(Save->DoorOpen[Index]);
			}
		}
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCSAVE] loaded seed=%llu doors=%d"),
		Save->Seed, Save->DoorOpen.Num());
	return true;
}
