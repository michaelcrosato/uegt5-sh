#include "UI/FCHUD.h"

#include "AI/FCDirectorSubsystem.h"
#include "AI/FCWatcher.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/FCInteractable.h"
#include "Interaction/FCInteractionComponent.h"
#include "Objectives/FCRunSubsystem.h"
#include "Player/FCPlayerCharacter.h"
#include "Testing/FCCitySubsystem.h"
#include "Testing/FCGenBuildingSubsystem.h"
#include "UI/FCShellSubsystem.h"

static TAutoConsoleVariable<int32> CVarFCHUDControls(
	TEXT("fc.HUD.Controls"),
	1,
	TEXT("Contextual control strip at the bottom (ROADMAP 11.3): 1 on, 0 off."));

void AFCHUD::DrawCenteredLine(const FString& Text, const float Y,
	const FLinearColor& Color, const float Scale, const bool bShadow)
{
	float Width = 0.0f;
	float Height = 0.0f;
	GetTextSize(Text, Width, Height, GEngine->GetLargeFont(), Scale);
	const float X = (Canvas->SizeX - Width) * 0.5f;
	if (bShadow)
	{
		// Opaque black plate behind the line (playtest: a drop shadow alone
		// still washed out against the flashlight disc) - always readable,
		// over glare and over pitch dark alike.
		constexpr float PadX = 10.0f;
		constexpr float PadY = 5.0f;
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.92f),
			X - PadX, Y - PadY, Width + 2.0f * PadX, Height + 2.0f * PadY);
	}
	DrawText(Text, Color, X, Y, GEngine->GetLargeFont(), Scale);
}

uint64 AFCHUD::ResolveSeed() const
{
	if (const UFCCitySubsystem* CitySub = GetWorld()->GetSubsystem<UFCCitySubsystem>())
	{
		if (CitySub->GetCityData().Lots.Num() > 0)
		{
			return CitySub->GetCityData().Seed;
		}
	}
	if (const UFCGenBuildingSubsystem* Gen = GetWorld()->GetSubsystem<UFCGenBuildingSubsystem>())
	{
		return Gen->GetBuildingData().Seed;
	}
	return 0;
}

void AFCHUD::DrawHUD()
{
	Super::DrawHUD();
	if (Canvas == nullptr || GEngine == nullptr)
	{
		return;
	}

	const AFCPlayerCharacter* Player = Cast<AFCPlayerCharacter>(GetOwningPawn());
	const UFCRunSubsystem* Run = GetWorld()->GetSubsystem<UFCRunSubsystem>();
	const float CenterY = Canvas->SizeY * 0.5f;

	// --- The shell screens own the frame while active ---
	const UFCShellSubsystem* Shell = GetGameInstance() != nullptr
		? GetGameInstance()->GetSubsystem<UFCShellSubsystem>() : nullptr;
	if (Shell != nullptr && Shell->IsMenuActive())
	{
		DrawRect(FLinearColor(0.015f, 0.02f, 0.035f, 1.0f), 0, 0, Canvas->SizeX, Canvas->SizeY);
		const FLinearColor Sodium(0.9f, 0.58f, 0.2f);
		const FLinearColor Dim(0.5f, 0.48f, 0.42f);
		const FLinearColor Bright(0.92f, 0.9f, 0.82f);

		switch (Shell->GetState())
		{
		case EFCShellState::Title:
		{
			DrawCenteredLine(TEXT("F O O T C A N D L E"), CenterY - 150, Sodium, 3.2f);
			DrawCenteredLine(TEXT("stay quiet. stay dark. get out."), CenterY - 96, Dim, 1.1f);
			const TCHAR* Items[] = { TEXT("Start"), TEXT("Settings"), TEXT("Quit") };
			for (int32 Index = 0; Index < 3; ++Index)
			{
				const bool bSelected = Shell->GetTitleIndex() == Index;
				DrawCenteredLine(bSelected
					? FString::Printf(TEXT("> %s <"), Items[Index]) : FString(Items[Index]),
					CenterY - 8 + Index * 40, bSelected ? Bright : Dim, bSelected ? 1.5f : 1.3f);
			}
			DrawCenteredLine(TEXT("arrows + Enter    ·    a new city every run"),
				Canvas->SizeY * 0.9f, Dim, 0.95f);
			break;
		}
		case EFCShellState::Settings:
		{
			DrawCenteredLine(TEXT("SETTINGS"), Canvas->SizeY * 0.14f, Sodium, 1.9f);
			const TArray<UFCShellSubsystem::FSettingsItem>& Items = Shell->GetSettingsItems();
			const float RowH = 34.0f;
			const float StartY = Canvas->SizeY * 0.26f;
			for (int32 Index = 0; Index < Items.Num(); ++Index)
			{
				const bool bSelected = Shell->GetSelectedIndex() == Index;
				const FLinearColor RowColor = bSelected ? Bright : Dim;
				const float Y = StartY + Index * RowH;
				DrawText(bSelected ? FString::Printf(TEXT("> %s"), *Items[Index].Label) : Items[Index].Label,
					RowColor, Canvas->SizeX * 0.3f, Y, GEngine->GetLargeFont(), bSelected ? 1.2f : 1.1f);
				DrawText(FString::Printf(TEXT("< %s >"), *Items[Index].GetValue()),
					RowColor, Canvas->SizeX * 0.58f, Y, GEngine->GetLargeFont(), bSelected ? 1.2f : 1.1f);
			}
			DrawCenteredLine(TEXT("arrows change    ·    Esc back"),
				Canvas->SizeY * 0.9f, Dim, 0.95f);
			break;
		}
		case EFCShellState::Intro:
		{
			DrawCenteredLine(TEXT("THE DISTRICT"), Canvas->SizeY * 0.16f, Sodium, 1.9f);
			const TCHAR* Lines[] = {
				TEXT("The city was evacuated years ago. Nobody told the grid -"),
				TEXT("the streetlights still run on their timers."),
				TEXT("Something moved in after everyone left."),
				TEXT("It hunts what it sees, and what it hears."),
				TEXT(""),
				TEXT("A key is hidden high in one of the buildings."),
				TEXT("The street power is dead - the extraction pad needs it back."),
				TEXT("Reach the pad. Survive the wait."),
				TEXT(""),
				TEXT("Darkness hides you. Light betrays you. Every footstep is information."),
				TEXT("Hold F for the quiet way.  Hold Alt to stop and listen."),
				TEXT("The red eye means it knows."),
			};
			float Y = Canvas->SizeY * 0.28f;
			for (const TCHAR* Line : Lines)
			{
				DrawCenteredLine(Line, Y, FLinearColor(0.78f, 0.75f, 0.68f), 1.05f);
				Y += 30.0f;
			}
			DrawCenteredLine(TEXT("[F] step into the street"), Canvas->SizeY * 0.88f, Bright, 1.25f);
			break;
		}
		default:
			break;
		}
		return;
	}

	// --- Full-screen cards: death teaches, victory credits the seed ---
	if (Player != nullptr && Player->GetHealthState() == EFCHealthState::Dead)
	{
		DrawRect(FLinearColor(0, 0, 0, 0.82f), 0, 0, Canvas->SizeX, Canvas->SizeY);
		DrawCenteredLine(TEXT("IT FOUND YOU"), CenterY - 70, FLinearColor(0.9f, 0.15f, 0.1f), 2.6f);
		DrawCenteredLine(FString::Printf(TEXT("seed %llu"), ResolveSeed()),
			CenterY + 6, FLinearColor(0.75f, 0.72f, 0.65f), 1.2f);
		DrawCenteredLine(TEXT("same seed, same city - it will be waiting"),
			CenterY + 34, FLinearColor(0.55f, 0.52f, 0.48f), 1.0f);
		DrawCenteredLine(TEXT("[R] go back in      [F10] quit"),
			CenterY + 78, FLinearColor(0.75f, 0.72f, 0.65f), 1.05f);
		return;
	}
	if (Run != nullptr && Run->IsWon())
	{
		DrawRect(FLinearColor(0, 0, 0, 0.75f), 0, 0, Canvas->SizeX, Canvas->SizeY);
		DrawCenteredLine(TEXT("YOU GOT OUT"), CenterY - 70, FLinearColor(0.35f, 1.0f, 0.55f), 2.6f);
		DrawCenteredLine(FString::Printf(TEXT("seed %llu"), ResolveSeed()),
			CenterY + 6, FLinearColor(0.75f, 0.72f, 0.65f), 1.2f);
		return;
	}
	if (GetWorld()->IsPaused())
	{
		DrawRect(FLinearColor(0, 0, 0, 0.6f), 0, 0, Canvas->SizeX, Canvas->SizeY);
		DrawCenteredLine(TEXT("PAUSED"), CenterY - 30, FLinearColor(0.9f, 0.87f, 0.8f), 2.0f);
		DrawCenteredLine(TEXT("[Esc] resume      [F2] settings      [F10] quit"),
			CenterY + 16, FLinearColor(0.6f, 0.58f, 0.52f), 1.05f);
		return;
	}
	if (Player == nullptr)
	{
		return;
	}

	// --- Interaction verb, center-low ---
	if (const UFCInteractionComponent* Interaction = Player->FindComponentByClass<UFCInteractionComponent>())
	{
		if (const AActor* Target = Interaction->GetCurrentTarget())
		{
			const FString Verb = Cast<IFCInteractable>(Target)->GetInteractionVerb();
			// Mint-cyan on a black drop shadow: distinct from every light
			// color in the game (sodium amber, warm bulbs, red LEDs) and the
			// shadow keeps it alive over both glare and pitch dark.
			DrawCenteredLine(FString::Printf(TEXT("[F] %s"), *Verb),
				CenterY + Canvas->SizeY * 0.12f, FLinearColor(0.4f, 0.95f, 0.8f), 1.15f,
				/*bShadow*/ true);
		}
	}

	// --- Soft meters: visible only when they matter (ROADMAP 11.2) ---
	float SoftY = Canvas->SizeY * 0.86f;
	if (Player->IsFlashlightOn() || Player->GetBattery() < 30.0f)
	{
		const FLinearColor Color = Player->GetBattery() < 20.0f
			? FLinearColor(1.0f, 0.35f, 0.2f) : FLinearColor(0.8f, 0.77f, 0.68f);
		DrawText(FString::Printf(TEXT("battery %.0f%%"), Player->GetBattery()),
			Color, Canvas->SizeX * 0.055f, SoftY, GEngine->GetLargeFont(), 1.0f);
		SoftY -= 22.0f;
	}
	if (Player->GetStamina() < 50.0f)
	{
		DrawText(FString::Printf(TEXT("breath %.0f%%"), Player->GetStamina()),
			FLinearColor(0.8f, 0.77f, 0.68f), Canvas->SizeX * 0.055f, SoftY,
			GEngine->GetLargeFont(), 1.0f);
	}
	if (Run != nullptr && Run->GetConditionsRequired() > 1)
	{
		DrawText(FString::Printf(TEXT("%d / %d"), Run->GetConditionsSatisfied(), Run->GetConditionsRequired()),
			FLinearColor(0.6f, 0.58f, 0.52f), Canvas->SizeX * 0.92f, Canvas->SizeY * 0.06f,
			GEngine->GetLargeFont(), 1.1f);
	}

#if !UE_BUILD_SHIPPING
	// --- Dev overlay (fc.DevMode, playtest ask) ---
	{
		static const auto DevModeVar = IConsoleManager::Get().FindConsoleVariable(TEXT("fc.DevMode"));
		if (DevModeVar != nullptr && DevModeVar->GetInt() != 0)
		{
			const float X = 18.0f;
			float Y = 18.0f;
			auto DevLine = [&](const FString& Text, const FLinearColor& Color = FLinearColor(0.4f, 1.0f, 0.6f))
			{
				DrawText(Text, Color, X, Y, GEngine->GetLargeFont(), 0.95f);
				Y += 19.0f;
			};
			DevLine(TEXT("== DEV (F1) =="), FLinearColor(1.0f, 0.85f, 0.3f));
			DevLine(FString::Printf(TEXT("seed %llu   fps %.0f"), ResolveSeed(),
				1.0f / FMath::Max(GetWorld()->GetDeltaSeconds(), 0.001f)));
			DevLine(FString::Printf(TEXT("pos %s"), *Player->GetActorLocation().ToCompactString()));
			DevLine(FString::Printf(TEXT("stam %.0f  batt %.0f  floor %.0f  %s%s"),
				Player->GetStamina(), Player->GetBattery(), Player->GetPassiveNoiseFloor(),
				Player->bGodMode ? TEXT("GOD ") : TEXT(""),
				Player->bGhostMode ? TEXT("GHOST") : TEXT("")));
			if (const UFCDirectorSubsystem* Director = GetWorld()->GetSubsystem<UFCDirectorSubsystem>())
			{
				DevLine(FString::Printf(TEXT("pressure %.0f  hunter %s"),
					Director->GetPressure(), Director->IsHunterActive() ? TEXT("ACTIVE") : TEXT("-")));
			}
			for (TActorIterator<AFCWatcher> It(GetWorld()); It; ++It)
			{
				DevLine(FString::Printf(TEXT("watcher: state %d meter %.2f dist %.0f"),
					static_cast<int32>(It->GetState()), It->GetDetectionMeter(),
					FVector::Dist2D(It->GetActorLocation(), Player->GetActorLocation())));
			}
			if (const UFCCitySubsystem* CitySub = GetWorld()->GetSubsystem<UFCCitySubsystem>())
			{
				DevLine(FString::Printf(TEXT("detail lot %d / %d lots"),
					CitySub->GetDetailLot(), CitySub->GetCityData().Lots.Num()));
			}
			DevLine(TEXT("F3 ghost F4 god F5 cond F6 win F7 kill F8 watcher F9 draws"),
				FLinearColor(0.6f, 0.6f, 0.55f));
		}
	}
#endif

	// --- The contextual control strip (ROADMAP 11.3) ---
	if (CVarFCHUDControls.GetValueOnGameThread() != 0)
	{
		FString Strip = TEXT("[WASD] move   [Shift] sprint   [Ctrl] sneak   [C] crouch   [Q/E] lean   [T] light   [Alt] listen");
		if (Player->IsListening())
		{
			Strip = TEXT("[Alt] release breath ... listening");
		}
		DrawCenteredLine(Strip, Canvas->SizeY * 0.955f, FLinearColor(0.5f, 0.48f, 0.42f, 0.85f), 0.95f);
	}
}
