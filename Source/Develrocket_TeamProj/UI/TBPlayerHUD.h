#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "UI/InGame/TBLeaderboardTypes.h"
#include "TBPlayerHUD.generated.h"

class UAbilitySystemComponent;
class UTBPlayerHUDWidget;
class UTBPauseMenuWidget;
class UTBMatchWaitWidget;
class UTBInteractionComponent;
class UTBLobbyReadyWidget;
class UTBLeaderboardWidget;

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
	
	void ShowLeaderboard();
	void HideLeaderboard();

	// 피격 방향 표시 (맞은 본인 클라이언트에서 호출)
	void ShowDamageIndicator(const FVector& Origin, AActor* Source);

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
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTBLobbyReadyWidget> LobbyReadyWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI")
	TSubclassOf<UTBLeaderboardWidget> LeaderboardWidgetClass;
	
	UPROPERTY(Transient)
	TObjectPtr<UTBLeaderboardWidget> LeaderboardWidget;
	
	UPROPERTY(EditDefaultsOnly, Category = "UI", meta=(ClampMin = "0.1", Units = "s"))
	float LeaderboardRefreshInterval = 0.5f;

private:
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshResourceHUD();
	void HandleSkillCooldownTagChanged(const FGameplayTag Tag, int32 NewCount);
	void RefreshSkillCooldownHUD();
	void NotifyShotFired();
	
	void HandleFocusedActorChanged(AActor* NewFocus);

	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;

	TArray<TPair<FGameplayAttribute, FDelegateHandle>> AttributeBindings;
	TArray<TPair<FGameplayTag, FDelegateHandle>> SkillCooldownBindings;
	FTimerHandle SkillCooldownRefreshTimer;

	FDelegateHandle ShotFiredHandle;
	
	TWeakObjectPtr<UTBInteractionComponent> BoundInteraction; //캐릭터와 함께 사라질 수 있음
	FDelegateHandle FocusChangedHandle;
	
	TArray<FTBLeaderboardRowData> BuildLeaderboardRows() const;
	void RefreshLeaderboard();
	FTimerHandle LeaderboardRefreshTimer;
	
};
