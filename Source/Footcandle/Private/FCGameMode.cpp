#include "FCGameMode.h"

#include "GameFramework/SpectatorPawn.h"

AFCGameMode::AFCGameMode()
{
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}
