#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FCInteractable.h"
#include "FCKeyItem.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

// The M6 hand-wired key condition (replaced by M8's systemic conditions).
// A small glowing pickup: its own light makes finding it a light-read, and
// taking it DARKENS the room - taking the key costs you its glow.
UCLASS()
class FOOTCANDLE_API AFCKeyItem : public AActor, public IFCInteractable
{
	GENERATED_BODY()

public:
	AFCKeyItem();

	virtual void Interact(AFCPlayerCharacter* User, bool bQuiet) override;
	virtual FString GetInteractionVerb() const override { return TEXT("Take"); }

protected:
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UPointLightComponent> Glow;
};
