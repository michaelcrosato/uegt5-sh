#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCTestStationSubsystem.generated.h"

// A named camera position for visual verification.
USTRUCT()
struct FFCTestStation
{
	GENERATED_BODY()

	FName Name;
	FVector Location = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
};

// Visual-test backbone (docs/ROADMAP.md §13): named stations you can pop
// between instantly, and a scripted "tour" that visits every station, lets
// lighting settle, and writes one screenshot per station for offline review.
//
// Console:
//   fc.Station <name>       - teleport the player to a station
//   fc.Stations             - list registered stations
//   fc.Tour [OutDir] [quit] - screenshot every station, optionally quit after
// Command line:
//   -fctour=<OutDir>        - auto-run the tour after world start, then quit
//   -fcflashcheck           - with -fctour: kill every light in the world
//                             except the player's flashlight and tour with
//                             the beam on - the lighting-in-anger audit
UCLASS()
class FOOTCANDLE_API UFCTestStationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	void RegisterStation(FName Name, const FVector& Location, const FRotator& Rotation);
	bool TeleportToStation(FName Name) const;
	void StartTour(const FString& OutputDir, bool bQuitWhenDone);

	const TArray<FFCTestStation>& GetStations() const { return Stations; }

private:
	bool TickTour(float DeltaTime);
	void FinishTour();
	void ApplyFlashlightOnly();

	bool bFlashCheckPending = false;
	bool bFlashOnPending = false;

	TArray<FFCTestStation> Stations;

	// Tour state machine.
	enum class ETourPhase : uint8 { Idle, Settling, Sampling, Capturing };
	ETourPhase TourPhase = ETourPhase::Idle;
	int32 TourStationIndex = 0;
	int32 TourFramesRemaining = 0;
	bool bTourQuitWhenDone = false;
	FString TourOutputDir;
	FTSTicker::FDelegateHandle TourTickerHandle;

	// Per-station perf accumulation ([FCPERF] lines feed the 13.4 gates).
	double SumGameMs = 0.0, SumRenderMs = 0.0, SumRHIMs = 0.0, SumGPUMs = 0.0, SumFrameMs = 0.0;
	int32 SampleCount = 0;
};
