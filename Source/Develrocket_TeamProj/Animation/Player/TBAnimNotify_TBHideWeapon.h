#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "TBAnimNotify_TBHideWeapon.generated.h"

UCLASS(meta=(DisplayName = "Tb Hide Weapon"))
class DEVELROCKET_TEAMPROJ_API UTBAnimNotify_TBHideWeapon : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
private:
	static void SetWeaponsHidden(USkeletalMeshComponent* MeshComp, bool bHidden);
};
