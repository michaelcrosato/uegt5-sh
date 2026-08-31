#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FCInteractable.h"
#include "FCExtractZone.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

// Extraction as a COMMIT WINDOW, not a win-door (ROADMAP 4.1): starting it
// is loud (a hunter magnet), takes real seconds, and ends the run only if
// you survive the wait beside its beacon.
UCLASS()
class FOOTCANDLE_API AFCExtractZone : public AActor, public IFCInteractable
{
	GENERATED_BODY()

public:
	AFCExtractZone();

	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact(AFCPlayerCharacter* User, bool bQuiet) override;
	virtual FString GetInteractionVerb() const override;
	virtual bool CanInteract(const AFCPlayerCharacter* User) const override;

	float CommitSeconds = 6.0f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Beacon;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UPointLightComponent> BeaconLight;

private:
	bool bCommitting = false;
	float CommitRemaining = 0.0f;
	TWeakObjectPtr<AFCPlayerCharacter> Committer;
};
