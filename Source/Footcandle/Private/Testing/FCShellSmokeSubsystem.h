#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCShellSmokeSubsystem.generated.h"

// Shell-flow smoke: the flagless front door - Title (paused, captured) ->
// Start -> Intro (captured) -> Playing (city live) -> pause -> Settings
// (captured, a value adjusted and persisted). Run: -fcshellsmoke ONLY (no
// scene flags - that IS the test).
UCLASS()
class UFCShellSmokeSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

private:
	bool Tick(float DeltaTime);
	void Check(const TCHAR* Name, bool bCondition);
	void Shot(const TCHAR* Name);
	void Finish();

	FTSTicker::FDelegateHandle TickerHandle;
	float StepTime = 0.0f;
	int32 Step = 0;
	int32 PassCount = 0;
	int32 FailCount = 0;
};
