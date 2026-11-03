#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "TBMonsterAIController.generated.h"

class UAISenseConfig_Sight;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBMonsterAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	 ATBMonsterAIController();
	
protected:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);
	
	UPROPERTY(VisibleAnywhere, Category = "AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float SightRadius = 1500.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float LoseSightRadius = 1800.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "AI|Perception")
	float PeripheralVisionAngle = 60.f;
};