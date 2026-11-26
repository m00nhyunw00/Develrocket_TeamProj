#pragma once

#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "TBHunterAnimationData.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequence;
class USkeletalMesh;

UENUM(BlueprintType)
enum class ETBHunterSwordAnimation : uint8 { Idle, Run, Draw, ApproachSlash, SlashOne, SlashTwo, Backstep, Hit, Death };

UCLASS(BlueprintType)
class DEVELROCKET_TEAMPROJ_API UTBHunterAnimationData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Animation")
	TSoftObjectPtr<USkeletalMesh> Mesh;

	// The rifle and sword AnimBPs own their respective locomotion Blend Spaces.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Animation")
	TSoftClassPtr<UAnimInstance> AnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Animation")
	TSoftObjectPtr<UAnimMontage> RifleAttackMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Animation")
	TMap<ETBHunterSwordAnimation, TSoftObjectPtr<UAnimSequence>> SwordActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Animation")
	TMap<ETBHunterSwordAnimation, float> SwordHitFractions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Animation")
	FName SwordFullBodySlot = TEXT("DefaultSlot");
};
