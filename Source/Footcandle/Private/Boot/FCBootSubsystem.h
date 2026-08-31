#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FCBootSubsystem.generated.h"

// Boot-time capability report (docs/ROADMAP.md §6.1, test LGT-01).
//
// Logs an unambiguous [FCBOOT] banner naming the RHI and hardware ray tracing
// support so packaged-build logs are auditable evidence, not guesswork.
// ADR-0004: hardware RT is a hard requirement; when fc.RequireHWRT=1 (the
// packaged default from M10) an unsupported device gets a clear refusal
// instead of a silent software fallback.
UCLASS()
class UFCBootSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	bool SupportsHardwareRayTracing() const { return bHardwareRayTracing; }

private:
	bool bHardwareRayTracing = false;
};
