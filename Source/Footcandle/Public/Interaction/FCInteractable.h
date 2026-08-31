#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FCInteractable.generated.h"

class AFCPlayerCharacter;

UINTERFACE(MinimalAPI)
class UFCInteractable : public UInterface
{
	GENERATED_BODY()
};

// One interact key, two intents (ROADMAP 4.6): tap = normal/fast,
// hold = quiet/careful. Every interactable answers both.
class FOOTCANDLE_API IFCInteractable
{
	GENERATED_BODY()

public:
	virtual void Interact(AFCPlayerCharacter* User, bool bQuiet) = 0;

	// Short verb for the contextual overlay ("Open", "Hide", "Take").
	virtual FString GetInteractionVerb() const { return TEXT("Use"); }

	virtual bool CanInteract(const AFCPlayerCharacter* User) const { return true; }
};
