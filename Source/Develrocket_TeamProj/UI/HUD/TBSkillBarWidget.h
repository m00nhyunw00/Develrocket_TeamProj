#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBSkillBarWidget.generated.h"

class UTexture2D;
class UTBSkillSlotWidget;
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBSkillBarWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void SetSkill(int32 Index, UTexture2D* Icon, const FText& Key);
	void StartSkillCooldown(int32 Index, float Duration);
	
protected:
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBSkillSlotWidget> SkillSlot1;
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTBSkillSlotWidget> SkillSlot2;
	
	UTBSkillSlotWidget*GetSlot(int32 Index) const;
};