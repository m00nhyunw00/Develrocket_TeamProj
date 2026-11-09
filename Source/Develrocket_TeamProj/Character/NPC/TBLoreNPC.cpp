#include "Character/NPC/TBLoreNPC.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

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

void ATBLoreNPC::Interact_Implementation(AActor* InteractInstigator)
{
	// [서버] 지금은 바꿀 게임 상태 없음
	// 나중에: "이미 이야기를 들은 플레이어" 기록 등 서버가 알아야 하는 상태
	UE_LOG(LogTemp, Log, TEXT("[LoreNPC] Server: %s"), *GetNameSafe(InteractInstigator));
}

void ATBLoreNPC::InteractLocal_Implementation(AActor* InteractInstigator)
{
	// [상호작용한 플레이어의 클라이언트]
	// 4단계: 상호작용한 플레이어를 향해 돌아보기
	// 5단계: 세계관 UI(WBP_Lore) 열기
	UE_LOG(LogTemp, Log, TEXT("[LoreNPC] Local: %s"), *GetNameSafe(InteractInstigator));
}

FText ATBLoreNPC::GetInteractPrompt_Implementation() const
{
	return InteractPrompt;
}