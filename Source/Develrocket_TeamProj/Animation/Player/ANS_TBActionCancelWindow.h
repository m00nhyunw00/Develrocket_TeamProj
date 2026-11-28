#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "GameplayTagContainer.h"
#include "ANS_TBActionCancelWindow.generated.h"

// 몽타주의 지정 구간에서 허용할 후속 입력 설정
UCLASS(meta = (DisplayName = "Action Cancel Window"))
class DEVELROCKET_TEAMPROJ_API UANS_TBActionCancelWindow
	: public UAnimNotifyState
{
	GENERATED_BODY()

public:
	// 현재 동작을 취소하고 실행할 수 있는 어빌리티 태그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cancel", meta = (Categories = "Ability.Combat"))
	FGameplayTagContainer AllowedAbilityTags;

	// 이동 입력에 의한 현재 동작 취소 허용 여부
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cancel")
	bool bAllowMovementCancel = false;
};