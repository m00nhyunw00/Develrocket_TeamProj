#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TBAIMovementVisibilitySubsystem.generated.h"

class APlayerController;
class ACharacter;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBAIMovementVisibilitySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	void UpdateReport(APlayerController* Viewer, const TArray<AActor*>& VisibleActors, bool bComplete);

private:
	struct FViewerReport
	{
		double Time = 0.0;
		bool bComplete = false;
		TSet<TWeakObjectPtr<AActor>> VisibleActors;
	};
	void UpdateMovementIntervals();
	TMap<TWeakObjectPtr<APlayerController>, FViewerReport> Reports;
	TSet<TWeakObjectPtr<ACharacter>> ThrottledCharacters;
	FTimerHandle UpdateTimer;
};
