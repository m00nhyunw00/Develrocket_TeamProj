#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AttributeSet.h"
#include "TBPlayerHUD.generated.h"

class UAbilitySystemComponent;
class UTBPlayerHUDWidget;
class UTBPauseMenuWidget;
class UTBMatchWaitWidget;
class UTBInteractionComponent;

struct FOnAttributeChangeData;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBPlayerHUD : public AHUD
{
	GENERATED_BODY()

public:
	void BindAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindAbilitySystem();

	UFUNCTION(BlueprintCallable, Category = "UI")
	void TogglePauseMenu();
	
	void BindInteraction(UTBInteractionComponent* InInteraction);
	void UnBindInteraction();

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(
		const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTBPlayerHUDWidget> HUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UTBPlayerHUDWidget> HUDWidget;

	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTBPauseMenuWidget> PauseWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UTBPauseMenuWidget> PauseWidget;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTBMatchWaitWidget> MatchWaitWidgetClass;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshResourceHUD();
	void NotifyShotFired();
	
	void HandleFocusedActorChanged(AActor* NewFocus);

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeBindings;

	FDelegateHandle ShotFiredHandle;
	
	TWeakObjectPtr<UTBInteractionComponent> BoundInteraction; //캐릭터와 함께 사라질 수 있음
	FDelegateHandle FocusChangedHandle;
	
};