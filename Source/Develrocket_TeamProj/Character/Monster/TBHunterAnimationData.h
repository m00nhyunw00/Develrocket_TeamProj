#pragma once

#include "Engine/DataAsset.h"
#include "UObject/SoftObjectPtr.h"
#include "TBHunterAnimationData.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;

UENUM(BlueprintType)
enum class ETBHunterSwordAnimation : uint8 { Idle, Run, Draw, ApproachSlash, SlashOne, SlashTwo, Backstep, Hit, Death };

USTRUCT(BlueprintType)
struct FTBHunterDodgeAnimation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge")
	TObjectPtr<UAnimSequence> Sequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dodge", meta = (Units = "cm", ToolTip = "Negative moves left; positive moves right relative to the Hunter's facing."))
	float RightOffset = 0.f;
};

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

	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> SwordDodgeSequences;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Dodge")
	TArray<FTBHunterDodgeAnimation> SwordDodgeAnimations;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Dodge")
	TObjectPtr<UMaterialInterface> SwordMotionTrailMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hunter|Animation")
	FName SwordFullBodySlot = TEXT("DefaultSlot");
};
