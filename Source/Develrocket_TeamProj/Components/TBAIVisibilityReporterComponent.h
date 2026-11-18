#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TBAIVisibilityReporterComponent.generated.h"

// Reports this local player's screen coverage to the authoritative world.
UCLASS(ClassGroup=(AI))
class DEVELROCKET_TEAMPROJ_API UTBAIVisibilityReporterComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UTBAIVisibilityReporterComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ReportVisibleAI();
	UFUNCTION(Server, Unreliable)
	void ServerReportVisibleAI(const TArray<AActor*>& VisibleActors, bool bComplete);
	FTimerHandle ReportTimer;
};
