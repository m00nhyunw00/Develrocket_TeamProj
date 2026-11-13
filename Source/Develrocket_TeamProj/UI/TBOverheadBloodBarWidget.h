#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBOverheadBloodBarWidget.generated.h"

class UProgressBar;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBOverheadBloodBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Blood Bar")
	void SetBlood(float Blood, float MaxBlood);

protected:
	virtual void NativeOnInitialized() override;

	// A Blueprint child may provide a ProgressBar with this name.
	UPROPERTY(meta=(BindWidgetOptional), BlueprintReadOnly, Category="Blood Bar")
	TObjectPtr<UProgressBar> BloodBar;
};
