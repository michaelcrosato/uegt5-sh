#include "FCGameMode.h"

#include "GameFramework/SpectatorPawn.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/FCPlayerCharacter.h"

AFCGameMode::AFCGameMode()
{
	// -fcspectator: free camera for scripted visual tours and debugging.
	if (FParse::Param(FCommandLine::Get(), TEXT("fcspectator")))
	{
		DefaultPawnClass = ASpectatorPawn::StaticClass();
	}
	else
	{
		DefaultPawnClass = AFCPlayerCharacter::StaticClass();
	}
}
