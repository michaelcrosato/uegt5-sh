#include "Rendering/FCUpscalerSubsystem.h"

#include "Footcandle.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	void SetCVarInt(const TCHAR* Name, const int32 Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByCommandline);
		}
	}

	void SetCVarFloat(const TCHAR* Name, const float Value)
	{
		if (IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			Var->Set(Value, ECVF_SetByCommandline);
		}
	}

	FAutoConsoleCommand GFCUpscalerCmd(
		TEXT("fc.Upscaler"),
		TEXT("fc.Upscaler <off|tsr|dlaa|dlss-quality|dlss-balanced|dlss-perf> [rr]"),
		FConsoleCommandWithArgsDelegate::CreateLambda(
			[](const TArray<FString>& Args)
			{
				if (Args.Num() == 0 || GEngine == nullptr)
				{
					return;
				}
				UWorld* World = GEngine->GetCurrentPlayWorld();
				if (World == nullptr)
				{
					return;
				}
				if (UFCUpscalerSubsystem* Subsystem =
					World->GetGameInstance()->GetSubsystem<UFCUpscalerSubsystem>())
				{
					const bool bRR = Args.Contains(TEXT("rr"));
					Subsystem->ApplyMode(Args[0], bRR);
				}
			}));
}

void UFCUpscalerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FString Mode = TEXT("tsr"); // vendor-free baseline (ADR-0007)
	FParse::Value(FCommandLine::Get(), TEXT("fcupscaler="), Mode);
	const bool bRR = FParse::Param(FCommandLine::Get(), TEXT("fcrr"));
	ApplyMode(Mode, bRR);
}

void UFCUpscalerSubsystem::ApplyMode(const FString& Mode, const bool bRayReconstruction)
{
	// DLSS replaces the engine's Temporal Upscaler when r.NGX.DLSS.Enable=1;
	// quality mode maps through r.ScreenPercentage (66.7 Quality / 58
	// Balanced / 50 Performance / 100 DLAA).
	float ScreenPercentage = 100.0f;
	int32 DLSSEnable = 0;

	if (Mode == TEXT("dlaa"))
	{
		DLSSEnable = 1;
		ScreenPercentage = 100.0f;
	}
	else if (Mode == TEXT("dlss-quality"))
	{
		DLSSEnable = 1;
		ScreenPercentage = 66.666f;
	}
	else if (Mode == TEXT("dlss-balanced"))
	{
		DLSSEnable = 1;
		ScreenPercentage = 58.0f;
	}
	else if (Mode == TEXT("dlss-perf"))
	{
		DLSSEnable = 1;
		ScreenPercentage = 50.0f;
	}
	else if (Mode == TEXT("tsr"))
	{
		// TSR stays the engine AA method (r.AntiAliasingMethod=4 in ini);
		// run it at the same internal res as DLSS Quality for honest A/Bs.
		ScreenPercentage = 66.666f;
	}
	else // off: native, TSR as AA at 100%
	{
		ScreenPercentage = 100.0f;
	}

	SetCVarInt(TEXT("r.NGX.DLSS.Enable"), DLSSEnable);
	SetCVarInt(TEXT("r.NGX.DLSS.DenoiserMode"), (DLSSEnable != 0 && bRayReconstruction) ? 1 : 0);
	SetCVarFloat(TEXT("r.ScreenPercentage"), ScreenPercentage);
	// Reflex Low Latency wherever the plugin supports it (all RTX).
	SetCVarInt(TEXT("t.Streamline.Reflex.Enable"), 1);

	UE_LOG(LogFootcandle, Display,
		TEXT("[FCUPSCALER] mode=%s dlss=%d rr=%d screenpct=%.1f"),
		*Mode, DLSSEnable, (DLSSEnable != 0 && bRayReconstruction) ? 1 : 0, ScreenPercentage);
}
