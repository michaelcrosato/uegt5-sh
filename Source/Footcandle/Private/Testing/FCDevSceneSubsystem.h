#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCDevSceneSubsystem.generated.h"

class AStaticMeshActor;

// Spawns a code-defined lighting test scene into an empty map (M0 visual
// proof, docs/ROADMAP.md M0/M2). Everything is spawned from C++ - no .umap
// content to author or review - and every light is movable and shadow-casting,
// exercising the MegaLights + Lumen HWRT path this game lives on.
//
// Trigger: -fcdevscene on the command line, or `fc.DevScene.Spawn` in console.
// Registers the M0 test stations with UFCTestStationSubsystem.
UCLASS()
class UFCDevSceneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void SpawnScene();

private:
	AStaticMeshActor* SpawnMesh(const TCHAR* MeshPath, const FVector& Location,
		const FRotator& Rotation, const FVector& Scale);

	bool bSpawned = false;
};
