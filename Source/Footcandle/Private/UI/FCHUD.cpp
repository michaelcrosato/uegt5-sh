#include "UI/FCHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/FCInteractable.h"
#include "Interaction/FCInteractionComponent.h"
#include "Objectives/FCRunSubsystem.h"
#include "Player/FCPlayerCharacter.h"
#include "Testing/FCCitySubsystem.h"
#include "Testing/FCGenBuildingSubsystem.h"

static TAutoConsoleVariable<int32> CVarFCHUDControls(
	TEXT("fc.HUD.Controls"),
	1,
	TEXT("Contextual control strip at the bottom (ROADMAP 11.3): 1 on, 0 off."));

void AFCHUD::DrawCenteredLine(const FString& Text, const float Y,
	const FLinearColor& Color, const float Scale)
{
	float Width = 0.0f;
	float Height = 0.0f;
	GetTextSize(Text, Width, Height, GEngine->GetLargeFont(), Scale);
	DrawText(Text, Color, (Canvas->SizeX - Width) * 0.5f, Y, GEngine->GetLargeFont(), Scale);
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
		DrawCenteredLine(TEXT("[Esc] resume      [F10] quit"),
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
			DrawCenteredLine(FString::Printf(TEXT("[F] %s"), *Verb),
				CenterY + Canvas->SizeY * 0.12f, FLinearColor(0.92f, 0.9f, 0.82f), 1.15f);
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
