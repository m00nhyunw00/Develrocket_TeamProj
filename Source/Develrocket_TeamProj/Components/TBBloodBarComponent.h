#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "TBBloodBarComponent.generated.h"

class UAbilitySystemComponent;
struct FOnAttributeChangeData;

// World-space blood bar bound to the owner's GAS attributes.
UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class DEVELROCKET_TEAMPROJ_API UTBBloodBarComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UTBBloodBarComponent();
	void BindAbilitySystem(UAbilitySystemComponent* InASC);
	void UnbindAbilitySystem();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Interval between local camera line-of-sight checks.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood Bar|Visibility", meta=(ClampMin="0.05", Units="s"))
	float VisibilityCheckInterval = 0.1f;

private:
	void HandleBloodChanged(const FOnAttributeChangeData& Data);
	void RefreshBloodBar();
	void UpdateBarVisibility();
	FTimerHandle VisibilityTimer;
	TWeakObjectPtr<UAbilitySystemComponent> BoundASC;
	FDelegateHandle BloodChangedHandle;
	FDelegateHandle MaxBloodChangedHandle;
};
