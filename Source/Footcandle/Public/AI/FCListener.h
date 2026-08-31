#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "FCListener.generated.h"

class UCapsuleComponent;
class UFloatingPawnMovement;
class UPointLightComponent;
class UStaticMeshComponent;
struct FFCNoiseEvent;

// THE LISTENER - the sound hunter (ROADMAP 8.2, second archetype). Nearly
// blind; hears like an array through the same portal graph as the player's
// ears (P5). Its law: it MOVES WHEN THE WORLD IS LOUD and FREEZES WHEN IT
// IS QUIET - so rain is cover for you and speed for it, and a silent clear
// night is the hardest weather the game has. Zero animation clips: slide
// and stop; the freeze IS the animation.
UCLASS()
class FOOTCANDLE_API AFCListener : public APawn
{
	GENERATED_BODY()

public:
	AFCListener();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void SetPatrolPoints(const TArray<FVector>& Points) { PatrolPoints = Points; }
	bool CanMoveNow() const;

	// Tuning.
	float HearingThreshold = 4.0f;
	float SlideSpeed = 260.0f;
	float ContactRange = 120.0f;
	float MoveWindowSeconds = 4.0f; // stays mobile this long after world noise
	float FloorMobilityThreshold = 8.0f; // ambient floor that keeps it moving

protected:
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UCapsuleComponent> Capsule;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Dish;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UPointLightComponent> EyeLight;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UFloatingPawnMovement> Movement;

private:
	void OnNoise(const FFCNoiseEvent& Event);

	FVector InterestPoint = FVector::ZeroVector;
	bool bHasInterest = false;
	float LastWorldNoiseTime = -100.0f;
	int32 PatrolIndex = 0;
	TArray<FVector> PatrolPoints;
	FDelegateHandle NoiseHandle;
};
