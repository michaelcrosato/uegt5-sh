#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FCInteractionComponent.generated.h"

class AFCPlayerCharacter;

// Center-screen interaction: traces from the camera, tracks the current
// target, and turns press/release timing into tap (normal) vs hold (quiet)
// intents (ROADMAP 4.6 hold-to-be-quiet).
UCLASS()
class FOOTCANDLE_API UFCInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFCInteractionComponent();

	void OnInteractPressed();
	void OnInteractReleased();

	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void UpdateTarget();
	void FireInteraction(bool bQuiet);

	TWeakObjectPtr<AActor> CurrentTarget;
	TWeakObjectPtr<AActor> PendingTarget;
	bool bHolding = false;
	bool bFiredQuiet = false;
	float HoldStartTime = 0.0f;
};
