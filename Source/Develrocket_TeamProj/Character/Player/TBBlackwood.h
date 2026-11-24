#pragma once

#include "CoreMinimal.h"
#include "Character/Player/TBPlayer.h"
#include "TBBlackwood.generated.h"

class UTBGameplayAbility_Empower;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBBlackwood : public ATBPlayer
{
	GENERATED_BODY()

public:
	ATBBlackwood(const FObjectInitializer& ObjectInitializer);
	
	virtual bool ConsumeAttackEnhancement(const FGameplayTag& AttackId, float& InOutDamage) override;

protected:
	virtual float ModifyIncomingAttackDamage(float Damage) const override;

private:
	UTBGameplayAbility_Empower* GetActiveEmpower() const;
};