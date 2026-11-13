#include "NPC/TBNPCBase.h"

#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

ATBNPCBase::ATBNPCBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// 캡슐: 루트 + 탐지/충돌 담당 (사람 크기)
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	SetRootComponent(Capsule);
	Capsule->InitCapsuleSize(34.0f, 88.0f);
	Capsule->SetCollisionProfileName(TEXT("BlockAllDynamic"));   // Visibility Block 포함

	// 메시: 외형만, 캡슐 바닥에 발이 닿도록 반높이만큼 내림
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Capsule);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));

	InteractPrompt = FText::FromString(TEXT("상호작용"));
}

bool ATBNPCBase::CanInteract_Implementation(AActor* InteractInstigator) const
{
	return bCanInteract;
}

void ATBNPCBase::Interact_Implementation(AActor* InteractInstigator)
{
	// [서버] 기본: 바꿀 게임 상태 없음 → 자식이 override
}

void ATBNPCBase::InteractLocal_Implementation(AActor* InteractInstigator)
{
	// [상호작용한 클라] 기본: 로컬 반응 없음 → 필요한 자식만 override
	PlayReactMontage();
}

FText ATBNPCBase::GetInteractPrompt_Implementation() const
{
	return InteractPrompt;
}

UAnimMontage* ATBNPCBase::GetInteractMontage_Implementation() const
{
	return InteractMontage;
}

UAnimMontage* ATBNPCBase::GetInteractFirstPersonMontage_Implementation() const
{
	return InteractFirstPersonMontage;
}

bool ATBNPCBase::ShouldResolveOnStart_Implementation() const
{
	// NPC 상호작용은 시작 즉시 결과 (흡혈처럼 끝까지 기다리지 않음)
	return true;
}

void ATBNPCBase::PlayReactMontage()
{
	if (!ReactMontage || !Mesh)
	{
		return;
	}

	// 몽타주는 ABP의 Slot 노드를 통해서만 재생됨 → AnimInstance(ABP)가 있어야 함
	UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NPC] %s: ABP가 없어 반응 몽타주를 재생할 수 없음"), *GetName());
		return;
	}

	// 연타해도 처음부터 다시 튀지 않게, 재생 중이면 무시
	if (AnimInstance->Montage_IsPlaying(ReactMontage))
	{
		return;
	}

	AnimInstance->Montage_Play(ReactMontage);
}