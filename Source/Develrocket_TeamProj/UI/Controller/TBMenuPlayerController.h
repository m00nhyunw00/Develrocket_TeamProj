#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TBMenuPlayerController.generated.h"

class UTBMainMenuWidget;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBMenuPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TSubclassOf<UTBMainMenuWidget> MainMenuClass;
	
	UPROPERTY()
	TObjectPtr<UTBMainMenuWidget> MainMenuWidget;
};