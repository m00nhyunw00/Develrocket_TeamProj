#include "Character/NPC/TBLoreNPC.h"
#include "UI/Dialogue/TBDialogueWidget.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"


ATBLoreNPC::ATBLoreNPC()
{
	PrimaryActorTick.bCanEverTick = false;

	// 캡슐: 루트 + 탐지/충돌 담당 (사람 크기, Character 기본값과 비슷하게)
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(34.0f, 88.0f);
	Capsule->SetCollisionProfileName(TEXT("BlockAllDynamic"));   // Visibility Block 포함

	// 메시: 외형만, 캡슐 바닥에 발이 닿도록 반높이만큼 내림
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));

	InteractPrompt = FText::FromString(TEXT("대화하기"));
}

bool ATBLoreNPC::CanInteract_Implementation(AActor* InteractInstigator) const
{
	return bCanInteract;
}

void ATBLoreNPC::InteractLocal_Implementation(AActor* InteractInstigator)
{
	// [상호작용한 플레이어의 클라이언트]

	// 이미 대화 중이면 새로 띄우지 않음
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

	// 4단계(돌아보기)가 나중에 여기에 붙음
}

FText ATBLoreNPC::GetInteractPrompt_Implementation() const
{
	return InteractPrompt;
}

void ATBLoreNPC::Interact_Implementation(AActor* InteractInstigator)
{
	// [서버] 지금은 바꿀 게임 상태 없음
	UE_LOG(LogTemp, Log, TEXT("[LoreNPC] Server: %s"), *GetNameSafe(InteractInstigator));
}