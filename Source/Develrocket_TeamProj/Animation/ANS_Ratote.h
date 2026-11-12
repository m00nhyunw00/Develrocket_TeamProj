#pragma once

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_Ratote.generated.h"

/**
 * 
 */
UCLASS(meta=(DisplayName = "Rotate To"))
class SOULLIKEGAME_API UANS_Ratote : public UAnimNotifyState
{
	GENERATED_BODY()
	
	
public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
