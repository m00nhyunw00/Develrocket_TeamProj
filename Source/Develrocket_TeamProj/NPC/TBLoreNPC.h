#pragma once

#include "CoreMinimal.h"
#include "NPC/TBNPCBase.h"
#include "TBLoreNPC.generated.h"

class UTBDialogueWidget;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBLoreNPC : public ATBNPCBase
{
	GENERATED_BODY()
	
public:
	ATBLoreNPC();
	
	virtual void Interact_Implementation(AActor* InteractInstigator) override;
	virtual void InteractLocal_Implementation(AActor* InteractInstigator) override;
	
protected:
	// 대화 -------------------
	
	// 대화를 띄울 위젯 
	UPROPERTY(EditDefaultsOnly, Category = "Dialogue")
	TSubclassOf<UTBDialogueWidget> DialogueWidgetClass;
	
	// 이 NPC의 대사
	UPROPERTY(EditAnywhere, Category = "Dialogue", meta=(MultiLine = true))
	TArray<FText> DialogueLines;
	
private:
	// 지금 떠 있는 대화 위젯 (중복 생성 방지)
	TWeakObjectPtr<UTBDialogueWidget> ActiveDialogue;
	
};