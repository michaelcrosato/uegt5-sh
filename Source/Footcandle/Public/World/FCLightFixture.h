#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/FCInteractable.h"
#include "World/FCFlickerComponent.h"
#include "FCLightFixture.generated.h"

class ULightComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

UENUM()
enum class EFCFixtureStyle : uint8
{
	CeilingBulb, // cord + bare bulb, warm point light
	Streetlight, // pole + arm + head, sodium down-spot (the city's identity)
	TV,          // wall panel, cold guttering glow
	EmergencyLED,// small red always-on marker
};

// EVERY gameplay light is a FIXTURE (director's rule + pillar P4): a visible
// body in the world - bulb, pole head, TV panel, LED - that can be turned
// ON, turned OFF, and DESTROYED. The bulb mesh sits at the light origin, so
// the real light illuminates its own housing: on reads as glowing, off as a
// dark object, broken as broken. A hard-enough prop impact shatters it
// (glass noise 95, permanent dark, a light-delta the Watcher notices).
UCLASS()
class FOOTCANDLE_API AFCLightFixture : public AActor, public IFCInteractable
{
	GENERATED_BODY()

public:
	AFCLightFixture();

	// Build the visible body + light for a style. Call right after spawn.
	void Configure(EFCFixtureStyle InStyle, const FLinearColor& Color, float IntensityCandela,
		float AttenuationRadius, EFCFlickerStyle Flicker = EFCFlickerStyle::MainsHum,
		bool bWithFlicker = false, uint64 FlickerSeed = 0);

	ULightComponent* GetLightComponent() const;
	EFCFixtureStyle GetStyle() const { return Style; }
	bool IsBroken() const { return bBroken; }
	bool IsOn() const;

	void Break(AActor* Breaker);

	// IFCInteractable - the fixture is its own switch.
	virtual void Interact(AFCPlayerCharacter* User, bool bQuiet) override;
	virtual FString GetInteractionVerb() const override;
	virtual bool CanInteract(const AFCPlayerCharacter* User) const override { return !bBroken; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Housing;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UStaticMeshComponent> Bulb;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<ULightComponent> Light;

	UPROPERTY(VisibleAnywhere, Category = "FC")
	TObjectPtr<UFCFlickerComponent> Flicker;

private:
	UFUNCTION()
	void OnBulbHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void SyncBulbAppearance();

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BulbMID;

	EFCFixtureStyle Style = EFCFixtureStyle::CeilingBulb;
	FLinearColor OnColor = FLinearColor::White;
	bool bBroken = false;
	bool bLastVisibleState = true;
	float SyncAccumulator = 0.0f;
};
