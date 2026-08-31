#pragma once

#include "CoreMinimal.h"
#include "FCBuildingGen.h"

class AFCDoor;
class UWorld;

// Building geometry from data, split along the streaming seam (ROADMAP 5.5):
//   SHELL  - perimeter walls (with openings), slabs, roof, stairs. Always
//            present while the building is in range: the skyline, the light
//            blockers, the silhouette.
//   DETAIL - interior walls, doors, lights, props, hideables. Spawned on
//            approach, despawned when far (the interior tier).
// The full streamer amortizes over frames later; the split is the contract.
struct FOOTCANDLE_API FFCSpawnedBuilding
{
	TArray<TWeakObjectPtr<AActor>> ShellActors;
	TArray<TWeakObjectPtr<AActor>> DetailActors;
	TArray<TObjectPtr<AFCDoor>> Doors; // detail-owned, generation order (save ids)
	TArray<int32> GraphPortalByGenPortal;
	bool bHasDetail = false;
};

namespace FC::Spawn
{
	FOOTCANDLE_API void SpawnShell(UWorld* World, const FC::Gen::FFCBuildingData& Building,
		const FVector& Origin, FFCSpawnedBuilding& Out);

	FOOTCANDLE_API void SpawnDetail(UWorld* World, const FC::Gen::FFCBuildingData& Building,
		const FVector& Origin, FFCSpawnedBuilding& Out);

	FOOTCANDLE_API void DespawnDetail(FFCSpawnedBuilding& Spawned);
}
