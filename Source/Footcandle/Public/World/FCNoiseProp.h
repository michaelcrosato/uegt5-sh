#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FCNoiseProp.generated.h"

class UStaticMeshComponent;

// A physics prop that turns impacts into noise events (pillar P5; physics as
// animation, ADR-0003). Chaos moves it; the impact impulse sets the loudness;
// a per-actor cooldown stops settling-rattle from spamming the AI (AUD-05).
UCLASS()
class FOOTCANDLE_API AFCNoiseProp : public AActor
{
	GENERATED_BODY()

public:
	AFCNoiseProp();

	void ConfigureMesh(const TCHAR* MeshPath, const FVector& Scale, float InBaseLoudness);

	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other,
		UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation,
		FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Mesh;

protected:
	float BaseLoudness = 45.0f;

private:
	float LastNoiseTime = -10.0f;
};
