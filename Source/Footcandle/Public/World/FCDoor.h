#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FCInteractable.h"
#include "FCDoor.generated.h"

class UStaticMeshComponent;

// A door is a rotating transform with a noise profile (ROADMAP 5.4, 9.1).
// Tap = normal swing (audible), hold = careful swing (quiet), and a fast
// swing that reaches the frame slams (loud). Closed, it is real geometry:
// it blocks light, sight, and - at M3 - sound through the portal graph.
UCLASS()
class FOOTCANDLE_API AFCDoor : public AActor, public IFCInteractable
{
	GENERATED_BODY()

public:
	AFCDoor();

	virtual void Tick(float DeltaSeconds) override;

	// IFCInteractable
	virtual void Interact(AFCPlayerCharacter* User, bool bQuiet) override;
	virtual FString GetInteractionVerb() const override;

	bool IsOpen() const { return TargetAngle > 5.0f; }

	// Degrees per second for the two intents; from tuning at spawn.
	float QuietSwingSpeed = 45.0f;
	float NormalSwingSpeed = 240.0f;

	// Acoustic portal binding (ROADMAP 7.2): the swinging leaf drives the
	// portal state the noise model and - at M4 - enemy hearing consume.
	void BindAcousticPortal(int32 InPortalId, bool bInExterior);

	// Save/load restore: snap to a state with no swing, no noise.
	void SetOpenInstant(bool bOpen);

protected:
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<USceneComponent> HingePivot;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Leaf;

private:
	void EmitDoorNoise(float Loudness) const;
	void UpdateAcousticPortal() const;

	float CurrentAngle = 0.0f;
	float TargetAngle = 0.0f;
	float SwingSpeed = 0.0f;
	bool bLastSwingWasFast = false;
	int32 PortalId = INDEX_NONE;
	bool bExteriorDoor = false;
};
