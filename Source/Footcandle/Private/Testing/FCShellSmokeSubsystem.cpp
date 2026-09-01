#include "Testing/FCShellSmokeSubsystem.h"

#include "Engine/World.h"
#include "Footcandle.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Testing/FCCitySubsystem.h"
#include "UI/FCShellSubsystem.h"
#include "UnrealClient.h"

#if !UE_BUILD_SHIPPING

void UFCShellSmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!InWorld.IsGameWorld() || !FParse::Param(FCommandLine::Get(), TEXT("fcshellsmoke")))
	{
		return;
	}
	UE_LOG(LogFootcandle, Display, TEXT("[FCSHELLSMOKE] starting"));
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UFCShellSmokeSubsystem::Tick));
}

void UFCShellSmokeSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	}
	Super::Deinitialize();
}

void UFCShellSmokeSubsystem::Check(const TCHAR* Name, const bool bCondition)
{
	if (bCondition)
	{
		++PassCount;
		UE_LOG(LogFootcandle, Display, TEXT("[FCSHELLSMOKE] PASS %s"), Name);
	}
	else
	{
		++FailCount;
		UE_LOG(LogFootcandle, Error, TEXT("[FCSHELLSMOKE] FAIL %s"), Name);
	}
}

void UFCShellSmokeSubsystem::Shot(const TCHAR* Name)
{
	FScreenshotRequest::RequestScreenshot(
		FPaths::ProjectSavedDir() / TEXT("VisualCheck") / FString::Printf(TEXT("FC_%s.png"), Name),
		false, false);
}

bool UFCShellSmokeSubsystem::Tick(const float DeltaTime)
{
	UWorld* World = GetWorld();
	UFCShellSubsystem* Shell = World->GetGameInstance() != nullptr
		? World->GetGameInstance()->GetSubsystem<UFCShellSubsystem>() : nullptr;
	UFCCitySubsystem* CitySub = World->GetSubsystem<UFCCitySubsystem>();
	if (Shell == nullptr || CitySub == nullptr)
	{
		return true;
	}
	StepTime += DeltaTime;

	switch (Step)
	{
	case 0: // flagless boot landed on the title, world paused, no city yet
		if (StepTime >= 2.0f)
		{
			Check(TEXT("Shell: flagless boot -> Title"), Shell->GetState() == EFCShellState::Title);
			Check(TEXT("Shell: world paused under the title"), World->IsPaused());
			Check(TEXT("Shell: city not spawned yet"), CitySub->GetCityData().Lots.Num() == 0);
			Shot(TEXT("Title"));
			Step = 1;
			StepTime = 0.0f;
		}
		break;

	case 1: // Start -> Intro over a spawned city
		if (StepTime >= 1.0f)
		{
			Shell->OnMenuConfirm(); // Start selected by default
			Check(TEXT("Shell: Start -> Intro"), Shell->GetState() == EFCShellState::Intro);
			Check(TEXT("Shell: city spawned behind the intro"), CitySub->GetCityData().Lots.Num() > 0);
			Step = 2;
			StepTime = 0.0f;
		}
		break;

	case 2: // capture intro, then step into the street
		if (StepTime >= 1.0f)
		{
			Shot(TEXT("Intro"));
			Step = 3;
			StepTime = 0.0f;
		}
		break;

	case 3:
		if (StepTime >= 1.0f)
		{
			Shell->OnMenuConfirm();
			Check(TEXT("Shell: Intro -> Playing, unpaused"),
				Shell->GetState() == EFCShellState::Playing && !World->IsPaused());
			// Settings from pause: adjust FOV and verify the cvar moved.
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->SetPause(true);
			}
			Shell->OpenSettingsFromPause();
			Check(TEXT("Shell: settings opened with items"),
				Shell->GetState() == EFCShellState::Settings && Shell->GetSettingsItems().Num() >= 8);
			const IConsoleVariable* FOVVar = IConsoleManager::Get().FindConsoleVariable(TEXT("fc.Camera.FOV"));
			const float Before = FOVVar != nullptr ? FOVVar->GetFloat() : 0.0f;
			Shell->OnMenuDown(); Shell->OnMenuDown(); // to Field of view
			Shell->OnMenuAdjust(+1);
			const float After = FOVVar != nullptr ? FOVVar->GetFloat() : 0.0f;
			Check(TEXT("Shell: adjusting FOV moved the cvar"), After > Before);
			Step = 4;
			StepTime = 0.0f;
		}
		break;

	case 4: // capture settings, close out
		if (StepTime >= 1.0f)
		{
			Shot(TEXT("Settings"));
			Step = 5;
			StepTime = 0.0f;
		}
		break;

	case 5:
		if (StepTime >= 1.0f)
		{
			Shell->OnMenuBack();
			Check(TEXT("Shell: back returns to the pause card"),
				Shell->GetState() == EFCShellState::Playing && World->IsPaused());
			Finish();
			return false;
		}
		break;

	default:
		break;
	}
	return true;
}

void UFCShellSmokeSubsystem::Finish()
{
	UE_LOG(LogFootcandle, Display, TEXT("[FCSHELLSMOKE] DONE pass=%d fail=%d"), PassCount, FailCount);
	FPlatformMisc::RequestExit(false);
}

#else

void UFCShellSmokeSubsystem::OnWorldBeginPlay(UWorld& InWorld) { Super::OnWorldBeginPlay(InWorld); }
void UFCShellSmokeSubsystem::Deinitialize() { Super::Deinitialize(); }
bool UFCShellSmokeSubsystem::Tick(float) { return false; }
void UFCShellSmokeSubsystem::Check(const TCHAR*, bool) {}
void UFCShellSmokeSubsystem::Shot(const TCHAR*) {}
void UFCShellSmokeSubsystem::Finish() {}

#endif
