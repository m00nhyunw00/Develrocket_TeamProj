#pragma once

#include "Character/Monster/TBMonster.h"
#include "TBVampireHunter.generated.h"

class UTBHunterAnimationData;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBVampireHunter : public ATBMonster
{
	GENERATED_BODY()

public:
	ATBVampireHunter(const FObjectInitializer& ObjectInitializer);
	const UTBHunterAnimationData* GetPhaseAnimationData(bool bSword) const;

protected:
	virtual bool TryDodgeRangedAttack() override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTBHunterAnimationData> RifleAnimationData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTBHunterAnimationData> SwordAnimationData;
};
