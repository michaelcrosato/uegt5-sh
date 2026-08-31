#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FCInteractable.h"
#include "FCHideSpot.generated.h"

class UStaticMeshComponent;

// Hide-in container (ROADMAP 5.4): interact to enter - camera cut, movement
// locked, constrained peek; interact again to exit to the stored position
// (clearance-checked, never traps the player - test battery item).
// No enter animation, by doctrine (ADR-0003).
UCLASS()
class FOOTCANDLE_API AFCHideSpot : public AActor, public IFCInteractable
{
	GENERATED_BODY()

public:
	AFCHideSpot();

	// IFCInteractable
	virtual void Interact(AFCPlayerCharacter* User, bool bQuiet) override;
	virtual FString GetInteractionVerb() const override;

	bool IsOccupied() const { return Occupant.IsValid(); }

protected:
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Shell;

	// Where the player stands while hidden (relative).
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<USceneComponent> HidePoint;

private:
	TWeakObjectPtr<AFCPlayerCharacter> Occupant;
	FVector ExitLocation = FVector::ZeroVector;
};
