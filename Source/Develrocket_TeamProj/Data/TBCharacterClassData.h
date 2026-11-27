#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBCharacterClassData.generated.h"

class ATBPlayer;
class UGameplayAbility;
class UGameplayEffect;
class UTexture2D;

// 직업 하나의 고정 데이터 (런타임에 수정하지 않는 읽기 전용 에셋)
UCLASS(BlueprintType)
class DEVELROCKET_TEAMPROJ_API UTBCharacterClassData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// 표시 ----------------------
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	// 작은 아이콘 (준비 목록·HUD용, 256x256)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UTexture2D> Icon;

	// 직업 선택 카드용 큰 그림 (2:3 세로)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UTexture2D> Portrait;

	// 폰 ----------------------
	UPROPERTY(EditDefaultsOnly, Category = "Pawn")
	TSubclassOf<ATBPlayer> PawnClass;

	// GAS ----------------------
	UPROPERTY(EditDefaultsOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	// 피해량 ----------------------
	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float PrimaryAttackDamage = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float SecondaryAttackDamage = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float MobilityAttackDamage = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float MeleeAttackDamage = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float Skill1Damage = 0.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float Skill2Damage = 0.0f;

	// 기본 공격 ----------------------
	UPROPERTY(EditDefaultsOnly, Category = "PrimaryAttack", meta = (ClampMin = "1.0"))
	float PrimaryAttackRange = 10000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "PrimaryAttack", meta = (ClampMin = "0.01"))
	float PrimaryAttackInterval = 0.1f;
};