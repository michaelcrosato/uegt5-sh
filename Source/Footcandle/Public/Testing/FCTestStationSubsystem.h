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

	TArray<FFCTestStation> Stations;

	// Tour state machine.
	enum class ETourPhase : uint8 { Idle, Settling, Capturing };
	ETourPhase TourPhase = ETourPhase::Idle;
	int32 TourStationIndex = 0;
	int32 TourFramesRemaining = 0;
	bool bTourQuitWhenDone = false;
	FString TourOutputDir;
	FTSTicker::FDelegateHandle TourTickerHandle;
};
