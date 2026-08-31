#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FCInteractable.h"
#include "FCLightSwitch.generated.h"

class ULightComponent;
class UStaticMeshComponent;

// A working light switch (pillar P4: every light is a gameplay object).
// M1: toggles its linked lights directly. M8 reroutes this through the
// power-grid circuit model without changing the interaction.
UCLASS()
class FOOTCANDLE_API AFCLightSwitch : public AActor, public IFCInteractable
{
	GENERATED_BODY()

public:
	AFCLightSwitch();

	void LinkLight(ULightComponent* Light);

	// IFCInteractable
	virtual void Interact(AFCPlayerCharacter* User, bool bQuiet) override;
	virtual FString GetInteractionVerb() const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Plate;

private:
	TArray<TWeakObjectPtr<ULightComponent>> LinkedLights;
	bool bOn = true;
};
