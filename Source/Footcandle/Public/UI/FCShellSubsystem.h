#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FCShellSubsystem.generated.h"

UENUM()
enum class EFCShellState : uint8
{
	Title,    // game name, start, settings (the front door)
	Settings, // the full options list
	Intro,    // premise + objective + basics before the run begins
	Playing,
};

// The shell state machine (playtest ask: title screen, real settings, an
// intro transition). Code-drawn via AFCHUD (ADR-0005); menu input routes
// from the player. The world sits PAUSED under Title/Settings/Intro.
// Settings persist to GameUserSettings.ini under [Footcandle.Settings].
UCLASS()
class FOOTCANDLE_API UFCShellSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// A settings row: cycle with left/right, rendered by the HUD.
	struct FSettingsItem
	{
		FString Label;
		TFunction<FString()> GetValue;
		TFunction<void(int32 /*Direction*/)> Adjust;
	};

	// Entered by the city subsystem on a flagless boot.
	void EnterTitle(class UFCCitySubsystem* InCity);

	EFCShellState GetState() const { return State; }
	bool IsMenuActive() const { return State != EFCShellState::Playing; }

	// Menu input (routed from the player's shell bindings).
	void OnMenuUp();
	void OnMenuDown();
	void OnMenuAdjust(int32 Direction);
	void OnMenuConfirm();
	void OnMenuBack();
	void OpenSettingsFromPause();

	const TArray<FSettingsItem>& GetSettingsItems() const { return SettingsItems; }
	int32 GetSelectedIndex() const { return SelectedIndex; }
	int32 GetTitleIndex() const { return TitleIndex; }

private:
	void BuildSettingsItems();
	void ApplyAndSave(const TCHAR* Key, const FString& Value);
	FString LoadSetting(const TCHAR* Key, const FString& Default) const;
	void ApplyAllSavedSettings();
	void SetPaused(bool bPaused) const;
	void BeginRun();
	void DismissIntro();

	EFCShellState State = EFCShellState::Playing;
	int32 TitleIndex = 0;    // 0 Start, 1 Settings, 2 Quit
	int32 SelectedIndex = 0; // settings row
	bool bSettingsFromPause = false;
	bool bSettingsBuilt = false;
	TArray<FSettingsItem> SettingsItems;
	TWeakObjectPtr<class UFCCitySubsystem> City;
};
