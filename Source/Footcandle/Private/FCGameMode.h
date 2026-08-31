#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FCGameMode.generated.h"

// Minimal M0 game mode: a spectator to fly the dev scene and test stations.
// Replaced by the real player pawn in M1.
UCLASS()
class AFCGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFCGameMode();
};
