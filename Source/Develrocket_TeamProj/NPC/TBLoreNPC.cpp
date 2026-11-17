#include "NPC/TBLoreNPC.h"
#include "UI/Dialogue/TBDialogueWidget.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

ATBLoreNPC::ATBLoreNPC()
{
	InteractPrompt = FText::FromString(TEXT("대화하기"));
}

void ATBLoreNPC::InteractLocal_Implementation(AActor* InteractInstigator)
{
	if (ActiveDialogue.IsValid() && ActiveDialogue->IsInViewport())
	{
		return;
	}
	
	if (!DialogueWidgetClass || DialogueLines.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LoreNPC] DialogueWidgetClass 또는 DialogueLines가 비어 있음"));
		return;
	}
	
	// 상호작용한 Pawn → 그 Pawn을 조종하는 로컬 PlayerController
	const APawn* InstigatorPawn = Cast<APawn>(InteractInstigator);
	APlayerController* PC = InstigatorPawn ? InstigatorPawn->GetController<APlayerController>() : nullptr;

	// 위젯은 내 화면에서만 만들 수 있음
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	// 위젯 생성 (소유자 = 이 PlayerController)
	UTBDialogueWidget* Dialogue = CreateWidget<UTBDialogueWidget>(PC, DialogueWidgetClass);
	if (!Dialogue)
	{
		return;
	}

	Dialogue->AddToViewport();

	// 화자 = 나(NPC), 청자 = 상호작용한 Pawn
	Dialogue->StartDialogue(this, InteractInstigator, DialogueLines);

	ActiveDialogue = Dialogue; 
}

void ATBLoreNPC::Interact_Implementation(AActor* InteractInstigator)
{
	// [서버] 지금은 바꿀 게임 상태 없음
	UE_LOG(LogTemp, Log, TEXT("[LoreNPC] Server: %s"), *GetNameSafe(InteractInstigator));
}

