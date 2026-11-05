#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBSessionRowWidget.generated.h"

class UButton;
class UTextBlock;
struct FTBSessionInfo;

UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBSessionRowWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	void Setup(const FTBSessionInfo& InInfo);
	
protected:
	virtual void NativeOnInitialized() override;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UButton> RowButton;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> HostNameText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> PlayerCountText;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UTextBlock> PingText;
	
private:
	UFUNCTION()
	void OnRowButtonClicked();
	
	int32 SearchIndex = INDEX_NONE;
};