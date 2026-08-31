#include "Boot/FCBootSubsystem.h"

#include "Footcandle.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "RHI.h"
#include "RenderUtils.h"

static TAutoConsoleVariable<int32> CVarFCRequireHWRT(
	TEXT("fc.RequireHWRT"),
	0,
	TEXT("1 = refuse to run without hardware ray tracing (ADR-0004). ")
	TEXT("0 during development so -nullrhi test runs still work."),
	ECVF_ReadOnly);

void UFCBootSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const TCHAR* RHIName = GDynamicRHI ? GDynamicRHI->GetName() : TEXT("<none>");
	bHardwareRayTracing = GRHISupportsRayTracing && GRHISupportsRayTracingShaders;

	UE_LOG(LogFootcandle, Display, TEXT("[FCBOOT] FOOTCANDLE %s | Engine %s"),
		FApp::GetBuildVersion(), *FEngineVersion::Current().ToString());
	UE_LOG(LogFootcandle, Display, TEXT("[FCBOOT] RHI=%s Feature=%s"),
		RHIName, *LexToString(GMaxRHIFeatureLevel));
	UE_LOG(LogFootcandle, Display, TEXT("[FCBOOT] HardwareRayTracing=%s (RHISupports=%d Shaders=%d RuntimeEnabled=%d)"),
		bHardwareRayTracing ? TEXT("SUPPORTED") : TEXT("UNSUPPORTED"),
		GRHISupportsRayTracing ? 1 : 0,
		GRHISupportsRayTracingShaders ? 1 : 0,
		IsRayTracingEnabled() ? 1 : 0);

	if (!bHardwareRayTracing && CVarFCRequireHWRT.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogFootcandle, Error,
			TEXT("[FCBOOT] This game requires a GPU with hardware ray tracing ")
			TEXT("(NVIDIA RTX 20-series, AMD RX 6000, Intel Arc, or newer). Exiting."));
		FPlatformMisc::RequestExit(false);
	}
}
