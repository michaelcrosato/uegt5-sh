#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FCHUD.generated.h"

// The dev shell (M10 core): a code-drawn HUD - no binary widget assets
// (ADR-0005). Ships the ROADMAP 11.2/11.3 contract in minimal form:
// near-empty by default, diegetic-first, a CONTEXTUAL control strip at the
// bottom (fc.HUD.Controls 0/1), soft battery/stamina readouts only when
// they matter, the interaction verb at center, and full-screen death /
// victory cards that always show THE SEED. CommonUI skinning is the polish
// pass; the information design lands here.
UCLASS()
class FOOTCANDLE_API AFCHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawCenteredLine(const FString& Text, float Y, const FLinearColor& Color, float Scale);
	uint64 ResolveSeed() const;
};
