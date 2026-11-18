#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ANS_TBMeleeTrace.generated.h"

// 애니메이션의 지정 구간에서 근접 판정 제어
UCLASS(meta = (DisplayName = "Melee Trace"))
class DEVELROCKET_TEAMPROJ_API UANS_TBMeleeTrace
	: public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshCompx,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};