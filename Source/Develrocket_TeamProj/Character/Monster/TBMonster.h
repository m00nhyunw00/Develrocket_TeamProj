#pragma once

#include "CoreMinimal.h"
#include "Character/TBCharacterBase.h"
#include "TBMonster.generated.h"

class UAbilitySystemComponent;

// 자체 ASC를 소유하는 AI 몬스터 캐릭터
UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBMonster : public ATBCharacterBase
{
	GENERATED_BODY()

public:
	ATBMonster(const FObjectInitializer& ObjectInitializer);

	virtual void PossessedBy(AController* NewController) override;

protected:
	virtual void BeginPlay() override;

	// 몬스터의 능력·효과·상태 태그를 관리하는 ASC
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> MonsterASC;
};
