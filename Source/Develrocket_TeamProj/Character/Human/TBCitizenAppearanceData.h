#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TBCitizenAppearanceData.generated.h"

class USkeletalMesh;

UCLASS(BlueprintType)
class DEVELROCKET_TEAMPROJ_API UTBCitizenAppearanceData : public UPrimaryDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
	TArray<TObjectPtr<USkeletalMesh>> SkeletalMesh;
};
