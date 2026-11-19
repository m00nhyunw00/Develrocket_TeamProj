#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TBLoadingScreenSubsystem.generated.h"

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBLoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
private:
	void OnPreLoadMap(const FString& Mapname);
	
	UPROPERTY()
	TSubclassOf<UUserWidget> LoadingWidgetClass;
};