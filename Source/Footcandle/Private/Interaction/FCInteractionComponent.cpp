#include "Interaction/FCInteractionComponent.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "FCTuningSettings.h"
#include "Footcandle.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/FCInteractable.h"
#include "Player/FCPlayerCharacter.h"

static TAutoConsoleVariable<int32> CVarFCInteractDebug(
	TEXT("fc.Interact.Debug"),
	0,
	TEXT("1 = on-screen current interaction target."));

UFCInteractionComponent::UFCInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFCInteractionComponent::UpdateTarget()
{
	CurrentTarget = nullptr;

	const AFCPlayerCharacter* Owner = Cast<AFCPlayerCharacter>(GetOwner());
	if (Owner == nullptr)
	{
		return;
	}
	const UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>();
	if (Camera == nullptr)
	{
		return;
	}

	const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * Tuning->InteractTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(FCInteract), true, GetOwner());
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor != nullptr && HitActor->Implements<UFCInteractable>())
		{
			if (Cast<IFCInteractable>(HitActor)->CanInteract(Owner))
			{
				CurrentTarget = HitActor;
			}
		}
	}
}

void UFCInteractionComponent::OnInteractPressed()
{
	UpdateTarget();
	PendingTarget = CurrentTarget;
	if (PendingTarget.IsValid())
	{
		bHolding = true;
		bFiredQuiet = false;
		HoldStartTime = GetWorld()->GetTimeSeconds();
	}
}

void UFCInteractionComponent::OnInteractReleased()
{
	if (bHolding && !bFiredQuiet)
	{
		FireInteraction(/*bQuiet*/ false);
	}
	bHolding = false;
}

void UFCInteractionComponent::FireInteraction(const bool bQuiet)
{
	if (AActor* Target = PendingTarget.Get())
	{
		if (Target->Implements<UFCInteractable>())
		{
			Cast<IFCInteractable>(Target)->Interact(
				Cast<AFCPlayerCharacter>(GetOwner()), bQuiet);
		}
	}
}

void UFCInteractionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Quiet interactions fire the moment the hold threshold is crossed - the
	// player feels the careful action begin under their finger.
	if (bHolding && !bFiredQuiet)
	{
		const UFCTuningSettings* Tuning = UFCTuningSettings::Get();
		if (GetWorld()->GetTimeSeconds() - HoldStartTime >= Tuning->QuietHoldSeconds)
		{
			bFiredQuiet = true;
			FireInteraction(/*bQuiet*/ true);
		}
	}
	else if (!bHolding)
	{
		UpdateTarget();
	}

	if (CVarFCInteractDebug.GetValueOnGameThread() != 0 && GEngine != nullptr)
	{
		const AActor* Target = CurrentTarget.Get();
		GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Cyan,
			Target != nullptr
				? FString::Printf(TEXT("[F] %s (%s)"),
					*Cast<IFCInteractable>(Target)->GetInteractionVerb(), *Target->GetName())
				: TEXT("--"));
	}
}
