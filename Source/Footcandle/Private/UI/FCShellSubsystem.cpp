#include "UI/FCShellSubsystem.h"

#include "Engine/World.h"
#include "Footcandle.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Rendering/FCUpscalerSubsystem.h"
#include "Testing/FCCitySubsystem.h"

namespace
{
	const TCHAR* SettingsSection = TEXT("Footcandle.Settings");

	void SetCVarFloatChecked(const TCHAR* Name, const float Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByGameSetting);
		}
	}

	float GetCVarFloatChecked(const TCHAR* Name, const float Default)
	{
		const IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
		return Var != nullptr ? Var->GetFloat() : Default;
	}

	const TCHAR* UpscalerModes[] = { TEXT("off"), TEXT("tsr"), TEXT("dlaa"),
		TEXT("dlss-quality"), TEXT("dlss-balanced"), TEXT("dlss-perf") };
	const TCHAR* UpscalerLabels[] = { TEXT("Off (native)"), TEXT("TSR"), TEXT("DLAA"),
		TEXT("DLSS Quality"), TEXT("DLSS Balanced"), TEXT("DLSS Performance") };
}

void UFCShellSubsystem::EnterTitle(UFCCitySubsystem* InCity)
{
	City = InCity;
	State = EFCShellState::Title;
	TitleIndex = 0;
	ApplyAllSavedSettings();
	SetPaused(true);
	UE_LOG(LogFootcandle, Display, TEXT("[FCSHELL] title"));
}

void UFCShellSubsystem::SetPaused(const bool bPaused) const
{
	if (APlayerController* PC = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->SetPause(bPaused);
	}
}

void UFCShellSubsystem::BeginRun()
{
	SetPaused(false);
	if (UFCCitySubsystem* CitySub = City.Get())
	{
		CitySub->SpawnAutoRun();
	}
	State = EFCShellState::Intro;
	SetPaused(true);
	UE_LOG(LogFootcandle, Display, TEXT("[FCSHELL] intro"));
}

void UFCShellSubsystem::DismissIntro()
{
	State = EFCShellState::Playing;
	SetPaused(false);
	UE_LOG(LogFootcandle, Display, TEXT("[FCSHELL] playing"));
}

void UFCShellSubsystem::OnMenuUp()
{
	if (State == EFCShellState::Title)
	{
		TitleIndex = (TitleIndex + 2) % 3;
	}
	else if (State == EFCShellState::Settings)
	{
		SelectedIndex = (SelectedIndex + SettingsItems.Num() - 1) % SettingsItems.Num();
	}
}

void UFCShellSubsystem::OnMenuDown()
{
	if (State == EFCShellState::Title)
	{
		TitleIndex = (TitleIndex + 1) % 3;
	}
	else if (State == EFCShellState::Settings)
	{
		SelectedIndex = (SelectedIndex + 1) % SettingsItems.Num();
	}
}

void UFCShellSubsystem::OnMenuAdjust(const int32 Direction)
{
	if (State == EFCShellState::Settings && SettingsItems.IsValidIndex(SelectedIndex))
	{
		SettingsItems[SelectedIndex].Adjust(Direction);
	}
}

void UFCShellSubsystem::OnMenuConfirm()
{
	switch (State)
	{
	case EFCShellState::Title:
		if (TitleIndex == 0)
		{
			BeginRun();
		}
		else if (TitleIndex == 1)
		{
			BuildSettingsItems();
			bSettingsFromPause = false;
			SelectedIndex = 0;
			State = EFCShellState::Settings;
		}
		else if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->ConsoleCommand(TEXT("quit"));
		}
		break;
	case EFCShellState::Settings:
		OnMenuAdjust(+1); // Enter cycles the selected value forward
		break;
	case EFCShellState::Intro:
		DismissIntro();
		break;
	default:
		break;
	}
}

void UFCShellSubsystem::OnMenuBack()
{
	if (State == EFCShellState::Settings)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
		if (bSettingsFromPause)
		{
			State = EFCShellState::Playing; // back to the pause card (still paused)
		}
		else
		{
			State = EFCShellState::Title;
		}
	}
	else if (State == EFCShellState::Intro)
	{
		DismissIntro();
	}
}

void UFCShellSubsystem::OpenSettingsFromPause()
{
	BuildSettingsItems();
	bSettingsFromPause = true;
	SelectedIndex = 0;
	State = EFCShellState::Settings;
}

void UFCShellSubsystem::ApplyAndSave(const TCHAR* Key, const FString& Value)
{
	GConfig->SetString(SettingsSection, Key, *Value, GGameUserSettingsIni);
}

FString UFCShellSubsystem::LoadSetting(const TCHAR* Key, const FString& Default) const
{
	FString Value;
	return GConfig->GetString(SettingsSection, Key, Value, GGameUserSettingsIni) ? Value : Default;
}

void UFCShellSubsystem::ApplyAllSavedSettings()
{
	// Upscaler + RR.
	const FString Mode = LoadSetting(TEXT("Upscaler"), TEXT("dlss-quality"));
	const bool bRR = LoadSetting(TEXT("RayReconstruction"), TEXT("0")) == TEXT("1");
	if (UFCUpscalerSubsystem* Upscaler = GetGameInstance()->GetSubsystem<UFCUpscalerSubsystem>())
	{
		Upscaler->ApplyMode(Mode, bRR);
	}
	// Scalar cvars.
	SetCVarFloatChecked(TEXT("fc.Camera.MotionScale"),
		FCString::Atof(*LoadSetting(TEXT("MotionScale"), TEXT("1.0"))));
	SetCVarFloatChecked(TEXT("fc.Flicker.Scale"),
		FCString::Atof(*LoadSetting(TEXT("FlickerScale"), TEXT("1.0"))));
	SetCVarFloatChecked(TEXT("fc.Camera.FOV"),
		FCString::Atof(*LoadSetting(TEXT("FOV"), TEXT("95"))));
	SetCVarFloatChecked(TEXT("fc.Input.Sensitivity"),
		FCString::Atof(*LoadSetting(TEXT("Sensitivity"), TEXT("1.0"))));
	SetCVarFloatChecked(TEXT("fc.Input.InvertY"),
		FCString::Atof(*LoadSetting(TEXT("InvertY"), TEXT("0"))));
	SetCVarFloatChecked(TEXT("fc.HUD.Controls"),
		FCString::Atof(*LoadSetting(TEXT("ControlHints"), TEXT("1"))));
}

void UFCShellSubsystem::BuildSettingsItems()
{
	if (bSettingsBuilt)
	{
		return;
	}
	bSettingsBuilt = true;

	// --- Upscaler ---
	SettingsItems.Add({ TEXT("Upscaler"),
		[this]()
		{
			const FString Mode = LoadSetting(TEXT("Upscaler"), TEXT("dlss-quality"));
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(UpscalerModes); ++Index)
			{
				if (Mode == UpscalerModes[Index])
				{
					return FString(UpscalerLabels[Index]);
				}
			}
			return Mode;
		},
		[this](const int32 Direction)
		{
			const FString Mode = LoadSetting(TEXT("Upscaler"), TEXT("dlss-quality"));
			int32 Current = 3;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(UpscalerModes); ++Index)
			{
				if (Mode == UpscalerModes[Index])
				{
					Current = Index;
				}
			}
			Current = (Current + Direction + UE_ARRAY_COUNT(UpscalerModes)) % UE_ARRAY_COUNT(UpscalerModes);
			ApplyAndSave(TEXT("Upscaler"), UpscalerModes[Current]);
			ApplyAllSavedSettings();
		} });

	// --- Ray Reconstruction ---
	SettingsItems.Add({ TEXT("Ray Reconstruction"),
		[this]() { return LoadSetting(TEXT("RayReconstruction"), TEXT("0")) == TEXT("1") ? FString(TEXT("On")) : FString(TEXT("Off")); },
		[this](const int32)
		{
			const bool bOn = LoadSetting(TEXT("RayReconstruction"), TEXT("0")) == TEXT("1");
			ApplyAndSave(TEXT("RayReconstruction"), bOn ? TEXT("0") : TEXT("1"));
			ApplyAllSavedSettings();
		} });

	// --- Scalar sliders ---
	auto AddSlider = [this](const TCHAR* Label, const TCHAR* Key, const TCHAR* CVar,
		const float Min, const float Max, const float Step, const TCHAR* Default)
	{
		SettingsItems.Add({ Label,
			[this, Key, Default]()
			{
				const float Value = FCString::Atof(*LoadSetting(Key, Default));
				return FMath::IsNearlyEqual(Value, FMath::RoundToFloat(Value), 0.001f)
					? FString::Printf(TEXT("%.0f"), Value)
					: FString::Printf(TEXT("%.2f"), Value);
			},
			[this, Key, CVar, Min, Max, Step, Default](const int32 Direction)
			{
				float Value = FCString::Atof(*LoadSetting(Key, Default));
				Value = FMath::Clamp(Value + Direction * Step, Min, Max);
				ApplyAndSave(Key, FString::SanitizeFloat(Value));
				SetCVarFloatChecked(CVar, Value);
			} });
	};
	AddSlider(TEXT("Field of view"), TEXT("FOV"), TEXT("fc.Camera.FOV"), 70.0f, 110.0f, 5.0f, TEXT("95"));
	AddSlider(TEXT("Mouse sensitivity"), TEXT("Sensitivity"), TEXT("fc.Input.Sensitivity"), 0.2f, 3.0f, 0.2f, TEXT("1.0"));
	AddSlider(TEXT("Camera motion"), TEXT("MotionScale"), TEXT("fc.Camera.MotionScale"), 0.0f, 1.0f, 0.25f, TEXT("1.0"));
	AddSlider(TEXT("Light flicker"), TEXT("FlickerScale"), TEXT("fc.Flicker.Scale"), 0.0f, 1.0f, 0.25f, TEXT("1.0"));

	// --- Toggles ---
	auto AddToggle = [this](const TCHAR* Label, const TCHAR* Key, const TCHAR* CVar, const TCHAR* Default)
	{
		SettingsItems.Add({ Label,
			[this, Key, Default]() { return LoadSetting(Key, Default) == TEXT("1") ? FString(TEXT("On")) : FString(TEXT("Off")); },
			[this, Key, CVar, Default](const int32)
			{
				const bool bOn = LoadSetting(Key, Default) == TEXT("1");
				ApplyAndSave(Key, bOn ? TEXT("0") : TEXT("1"));
				SetCVarFloatChecked(CVar, bOn ? 0.0f : 1.0f);
			} });
	};
	AddToggle(TEXT("Invert look Y"), TEXT("InvertY"), TEXT("fc.Input.InvertY"), TEXT("0"));
	AddToggle(TEXT("Control hints"), TEXT("ControlHints"), TEXT("fc.HUD.Controls"), TEXT("1"));

	// --- Window mode ---
	SettingsItems.Add({ TEXT("Window"),
		[]()
		{
			const UGameUserSettings* Settings = GEngine->GetGameUserSettings();
			return Settings->GetFullscreenMode() == EWindowMode::Windowed
				? FString(TEXT("Windowed")) : FString(TEXT("Fullscreen"));
		},
		[](const int32)
		{
			UGameUserSettings* Settings = GEngine->GetGameUserSettings();
			const bool bWindowed = Settings->GetFullscreenMode() == EWindowMode::Windowed;
			Settings->SetFullscreenMode(bWindowed ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed);
			Settings->ApplySettings(false);
		} });
}
