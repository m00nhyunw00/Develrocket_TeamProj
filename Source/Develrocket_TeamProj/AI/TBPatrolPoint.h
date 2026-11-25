#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "GameplayTagContainer.h"
#include "TBPatrolPoint.generated.h"

class UBillboardComponent;
class ANavigationData;
class UTBPatrolPointSubsystem;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBPatrolPoint : public ATargetPoint
{
	GENERATED_BODY()
	
public:
	ATBPatrolPoint();
	
	const FGameplayTag& GetPatrolZone() const{ return PatrolZone; }
	bool IsJunction() const;
	ATBPatrolPoint* ChooseNextPoint(const ANavigationData* NavData, const ATBPatrolPoint* Previous,
		const FVector& PreferredHeading, const ATBPatrolPoint* TemporarilyBlocked = nullptr, float PreferredLegLength = 0.0f) const;
	void ClearPatrolLinks();
	void AddPatrolLink(const ANavigationData* NavData, ATBPatrolPoint* Point);
	
protected:
	TMap<TWeakObjectPtr<const ANavigationData>, TArray<TWeakObjectPtr<ATBPatrolPoint>>> PatrolLinks;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY(EditInstanceOnly, Category="Patrol")
	FGameplayTag PatrolZone;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UBillboardComponent> EditorIcon;
#endif
};
