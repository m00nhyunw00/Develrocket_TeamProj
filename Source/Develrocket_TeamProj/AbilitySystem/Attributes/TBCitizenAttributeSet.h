#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "TBCitizenAttributeSet.generated.h"

DECLARE_MULTICAST_DELEGATE(FTBCitizenOutOfBlood);

// 시민 혈액(=체력) 관리, 무적·사망 중 혈액 감소 차단
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBCitizenAttributeSet : public UTBAttributeSetBase
{
	GENERATED_BODY()

public:
	UTBCitizenAttributeSet();

	// 무적·사망 상태에서 들어오는 혈액 감소 실행 거부 (피격·출혈 공통 관문)
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	// 혈액 0 도달 알림
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	FTBCitizenOutOfBlood OnOutOfBlood;   // [서버] 시민이 구독해서 사망 처리
};