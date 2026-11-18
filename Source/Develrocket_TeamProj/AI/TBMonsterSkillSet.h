#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "TBMonsterSkillSet.generated.h"

class UGameplayAbility;

USTRUCT(BlueprintType)
struct FTBMonsterSkillEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(ClampMin="0", Units="cm"))
	float MinRange = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(ClampMin="0", Units="cm"))
	float MaxRange = 200.f;

	// Higher values take precedence; equal priorities use the list order.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
	int32 Priority = 0;

	// Enable for abilities that need the target through TriggerEventData.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
	bool bUseTargetEvent = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(EditCondition="bUseTargetEvent"))
	FGameplayTag EventTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill")
	bool bStopMovement = false;
};

UCLASS(BlueprintType)
class DEVELROCKET_TEAMPROJ_API UTBMonsterSkillSet : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skills")
	TArray<FTBMonsterSkillEntry> Skills;
};
