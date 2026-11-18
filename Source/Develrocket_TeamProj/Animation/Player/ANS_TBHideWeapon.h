#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_TBHideWeapon.generated.h"

UCLASS(meta=(DisplayName = "Hide Weapon"))
class DEVELROCKET_TEAMPROJ_API UANS_TBHideWeapon : public UAnimNotifyState
{
	GENERATED_BODY()
	
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
private:
	static void SetWeaponsHidden(USkeletalMeshComponent* MeshComp, bool bHidden);
};
