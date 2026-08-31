#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FCInteractable.h"
#include "FCBreakerPanel.generated.h"

class ULightComponent;
class UStaticMeshComponent;

// A breaker: the power grid made touchable (ROADMAP 8.5). Throwing it is a
// LOUD mechanical event and a district-visible light delta - the strongest
// expression of "light is the whole game": you author the darkness you need,
// and everything that hunts by either channel hears you do it.
UCLASS()
class FOOTCANDLE_API AFCBreakerPanel : public AActor, public IFCInteractable
{
	GENERATED_BODY()

public:
	AFCBreakerPanel();

	void LinkLight(ULightComponent* Light);
	bool IsOn() const { return bOn; }
	void SetLabel(const FString& InLabel) { Label = InLabel; }
	void SetInitialOn(const bool bInitialOn) { bOn = bInitialOn; }

	// Optional: throwing this breaker satisfies a run condition (M8's
	// "restore power" objective) when it turns the circuit ON.
	bool bSatisfiesConditionWhenOn = false;

	virtual void Interact(AFCPlayerCharacter* User, bool bQuiet) override;
	virtual FString GetInteractionVerb() const override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Box;

private:
	TArray<TWeakObjectPtr<ULightComponent>> LinkedLights;
	FString Label = TEXT("Breaker");
	bool bOn = true;
	bool bConditionReported = false;
};
