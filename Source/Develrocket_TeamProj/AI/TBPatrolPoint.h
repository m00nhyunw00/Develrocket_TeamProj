#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "TBPatrolPoint.generated.h"

class UBillboardComponent;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBPatrolPoint : public AActor
{
	GENERATED_BODY()
	
public:
	ATBPatrolPoint();
	
	const FGameplayTag& GetPatrolZone() const{ return PatrolZone; }
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(EditInstanceOnly, Category="Patrol")
	FGameplayTag PatrolZone;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> EditorIcon;
#endif
};