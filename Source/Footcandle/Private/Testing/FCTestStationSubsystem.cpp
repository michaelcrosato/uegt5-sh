#include "Testing/FCTestStationSubsystem.h"

#include "Components/LightComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Footcandle.h"
#include "Player/FCPlayerCharacter.h"
#include "UObject/UObjectIterator.h"
#include "Misc/App.h"
#include "GPUProfiler.h"
#include "RenderCore.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

static TAutoConsoleVariable<int32> CVarFCTourSettleFrames(
	TEXT("fc.Tour.SettleFrames"),
	90,
	TEXT("Frames to wait after teleporting to a station before the screenshot, ")
	TEXT("so Lumen/TSR temporal accumulation settles."));

namespace
{
	FAutoConsoleCommandWithWorldAndArgs GFCStationCmd(
		TEXT("fc.Station"),
		TEXT("fc.Station <name> - teleport the player to a named test station."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World)
			{
				if (World == nullptr || Args.Num() == 0)
				{
					return;
				}
				if (UFCTestStationSubsystem* Subsystem = World->GetSubsystem<UFCTestStationSubsystem>())
				{
					if (!Subsystem->TeleportToStation(FName(*Args[0])))
					{
						UE_LOG(LogFootcandle, Warning, TEXT("[FCTEST] Unknown station '%s'"), *Args[0]);
					}
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GFCStationsCmd(
		TEXT("fc.Stations"),
		TEXT("List registered test stations."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>&, UWorld* World)
			{
				if (World == nullptr)
				{
					return;
				}
				if (const UFCTestStationSubsystem* Subsystem = World->GetSubsystem<UFCTestStationSubsystem>())
				{
					for (const FFCTestStation& Station : Subsystem->GetStations())
					{
						UE_LOG(LogFootcandle, Display, TEXT("[FCTEST] Station %s @ %s"),
							*Station.Name.ToString(), *Station.Location.ToCompactString());
					}
				}
			}));

	FAutoConsoleCommandWithWorldAndArgs GFCTourCmd(
		TEXT("fc.Tour"),
		TEXT("fc.Tour [OutDir] [quit] - screenshot every station."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args, UWorld* World)
			{
				if (World == nullptr)
				{
					return;
				}
				if (UFCTestStationSubsystem* Subsystem = World->GetSubsystem<UFCTestStationSubsystem>())
				{
					const FString OutDir = Args.Num() > 0
						? Args[0]
						: FPaths::ProjectSavedDir() / TEXT("VisualCheck");
					const bool bQuit = Args.ContainsByPredicate(
						[](const FString& Arg) { return Arg.Equals(TEXT("quit"), ESearchCase::IgnoreCase); });
					Subsystem->StartTour(OutDir, bQuit);
				}
			}));
}

void UFCTestStationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld())
	{
		return;
	}

	FString TourDir;
	if (FParse::Value(FCommandLine::Get(), TEXT("fctour="), TourDir))
	{
		// Auto-tour for scripted visual checks: give the dev scene a moment to
		// spawn and the first frame to render, then run and quit.
		StartTour(TourDir, /*bQuitWhenDone*/ true);
	}
}

void UFCTestStationSubsystem::Deinitialize()
{
	if (TourTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TourTickerHandle);
		TourTickerHandle.Reset();
	}
	Super::Deinitialize();
}

void UFCTestStationSubsystem::RegisterStation(const FName Name, const FVector& Location, const FRotator& Rotation)
{
	FFCTestStation& Station = Stations.AddDefaulted_GetRef();
	Station.Name = Name;
	Station.Location = Location;
	Station.Rotation = Rotation;
}

bool UFCTestStationSubsystem::TeleportToStation(const FName Name) const
{
	const FFCTestStation* Station = Stations.FindByPredicate(
		[Name](const FFCTestStation& Candidate) { return Candidate.Name == Name; });
	if (Station == nullptr)
	{
		return false;
	}

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC == nullptr)
	{
		return false;
	}

	if (APawn* Pawn = PC->GetPawn())
	{
		Pawn->TeleportTo(Station->Location, Station->Rotation, false, true);
	}
	PC->SetControlRotation(Station->Rotation);
	return true;
}

void UFCTestStationSubsystem::StartTour(const FString& OutputDir, const bool bQuitWhenDone)
{
	if (TourPhase != ETourPhase::Idle || Stations.Num() == 0)
	{
		if (Stations.Num() == 0)
		{
			UE_LOG(LogFootcandle, Warning, TEXT("[FCTEST] fc.Tour: no stations registered"));
			if (bQuitWhenDone)
			{
				FPlatformMisc::RequestExit(false);
			}
		}
		return;
	}

	TourOutputDir = OutputDir;
	bTourQuitWhenDone = bQuitWhenDone;
	TourStationIndex = 0;
	TourPhase = ETourPhase::Settling;
	TourFramesRemaining = CVarFCTourSettleFrames.GetValueOnGameThread();
	bFlashCheckPending = FParse::Param(FCommandLine::Get(), TEXT("fcflashcheck"));
	TeleportToStation(Stations[0].Name);
	UE_LOG(LogFootcandle, Display, TEXT("[FCTEST] Tour started: %d stations -> %s"),
		Stations.Num(), *TourOutputDir);

	TourTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCTestStationSubsystem::TickTour));
}

bool UFCTestStationSubsystem::TickTour(float /*DeltaTime*/)
{
	if (TourPhase == ETourPhase::Idle)
	{
		return false;
	}

	// Flash-check blackout runs on the first tour tick, after every scene
	// subsystem's BeginPlay has finished spawning its lights.
	if (bFlashCheckPending)
	{
		bFlashCheckPending = false;
		ApplyFlashlightOnly();
	}

	// While sampling, accumulate thread/GPU timings every frame.
	if (TourPhase == ETourPhase::Sampling)
	{
		SumFrameMs += FApp::GetDeltaTime() * 1000.0;
		SumGameMs += FPlatformTime::ToMilliseconds(GGameThreadTime);
		SumRenderMs += FPlatformTime::ToMilliseconds(GRenderThreadTime);
		SumRHIMs += FPlatformTime::ToMilliseconds(GRHIThreadTime);
		static FRHIGPUFrameTimeHistory::FState GPUTimeState;
		static double LastGPUMs = 0.0;
		uint64 GPUCycles64 = 0;
		while (GPUTimeState.PopFrameCycles(GPUCycles64) != FRHIGPUFrameTimeHistory::EResult::Empty)
		{
			LastGPUMs = FPlatformTime::ToMilliseconds64(GPUCycles64);
		}
		SumGPUMs += LastGPUMs;
		++SampleCount;
	}

	if (--TourFramesRemaining > 0)
	{
		return true;
	}

	if (TourPhase == ETourPhase::Settling)
	{
		// Settled: sample perf for 45 frames before the screenshot.
		SumGameMs = SumRenderMs = SumRHIMs = SumGPUMs = SumFrameMs = 0.0;
		SampleCount = 0;
		TourPhase = ETourPhase::Sampling;
		TourFramesRemaining = 45;
		return true;
	}

	if (TourPhase == ETourPhase::Sampling)
	{
		const FFCTestStation& Station = Stations[TourStationIndex];
		if (SampleCount > 0)
		{
			UE_LOG(LogFootcandle, Display,
				TEXT("[FCPERF] station=%s frame=%.2fms game=%.2fms render=%.2fms rhi=%.2fms gpu=%.2fms"),
				*Station.Name.ToString(),
				SumFrameMs / SampleCount, SumGameMs / SampleCount,
				SumRenderMs / SampleCount, SumRHIMs / SampleCount, SumGPUMs / SampleCount);
		}
		const FString Filename = TourOutputDir / FString::Printf(TEXT("FC_%s.png"), *Station.Name.ToString());
		FScreenshotRequest::RequestScreenshot(Filename, /*bInShowUI*/ false, /*bAddFilenameSuffix*/ false);
		UE_LOG(LogFootcandle, Display, TEXT("[FCTEST] Capturing %s"), *Filename);
		TourPhase = ETourPhase::Capturing;
		TourFramesRemaining = 15; // let the async write land
		return true;
	}

	// Capturing finished for this station; advance.
	++TourStationIndex;
	if (TourStationIndex >= Stations.Num())
	{
		FinishTour();
		return false;
	}

	TeleportToStation(Stations[TourStationIndex].Name);
	TourPhase = ETourPhase::Settling;
	TourFramesRemaining = CVarFCTourSettleFrames.GetValueOnGameThread();
	return true;
}

void UFCTestStationSubsystem::ApplyFlashlightOnly()
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GetWorld();
	APlayerController* PC = World->GetFirstPlayerController();
	AFCPlayerCharacter* Player = PC != nullptr ? Cast<AFCPlayerCharacter>(PC->GetPawn()) : nullptr;
	int32 Killed = 0;
	for (TObjectIterator<ULightComponent> It; It; ++It)
	{
		ULightComponent* Light = *It;
		if (Light->GetWorld() != World || (Player != nullptr && Light->GetOwner() == Player))
		{
			continue; // the flashlight (and only the flashlight) survives
		}
		if (Light->IsVisible())
		{
			Light->SetVisibility(false);
			++Killed;
		}
	}
	if (Player != nullptr)
	{
		Player->TestSetFlashlight(true);
	}
	UE_LOG(LogFootcandle, Display,
		TEXT("[FCTEST] flash-check: %d lights extinguished, flashlight forced on"), Killed);
#endif
}

void UFCTestStationSubsystem::FinishTour()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCTEST] Tour complete: %d screenshots in %s"),
		Stations.Num(), *TourOutputDir);
	TourPhase = ETourPhase::Idle;
	if (TourTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TourTickerHandle);
		TourTickerHandle.Reset();
	}
	if (bTourQuitWhenDone)
	{
		FPlatformMisc::RequestExit(false);
	}
}
