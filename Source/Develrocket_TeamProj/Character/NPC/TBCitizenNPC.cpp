#include "Character/NPC/TBCitizenNPC.h"

#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
// #include "Components/TBSpawnManagerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "Abilities/GameplayAbility.h"
#include "AI/AIControler/NPC/TBCitizenAIController.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "Components/TBHealComponent.h"
#include "TBGameplayTags.h"

// 생성자 =====================
ATBCitizenNPC::ATBCitizenNPC(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AIControllerClass = ATBCitizenAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->MaxWalkSpeed = 450.f;
	MoveComp->bOrientRotationToMovement = true;
	bUseControllerRotationYaw = false;

	InteractPrompt = FText::FromString(TEXT("흡혈하기"));

	AbilitySystemComponent = CreateDefaultSubobject<UTBAbilitySystemComponent>(TEXT("CitizenASC"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// 부모가 ASC 연결 시 복제 태그로 부여 (플레이어·몬스터와 같은 규칙)
	CharacterTypeTag = TBGameplayTags::Character_Type_Human;
}

void ATBCitizenNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBCitizenNPC, CitizenState);
}

void ATBCitizenNPC::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem(AbilitySystemComponent.Get(), this);

	// 사망 GA 부여 (서버 전용, AddCharacterAbilities가 권한·중복 검사를 함)
	if (HasAuthority() && DeathAbilityClass)
	{
		AbilitySystemComponent->AddCharacterAbilities({ DeathAbilityClass });
	}
}

void ATBCitizenNPC::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem(AbilitySystemComponent.Get(), this);
}

// 상호작용 =====================
bool ATBCitizenNPC::CanInteract_Implementation(AActor* InteractInstigator) const
{
	return CitizenState == ETBCitizenState::Idle;   // 흡혈 중이거나 죽었으면 다른 플레이어도 불가
}

void ATBCitizenNPC::BeginInteract_Implementation(AActor* InteractInstigator)
{
	if (!HasAuthority() || CitizenState != ETBCitizenState::Idle)
	{
		return;
	}

	DrainInstigator = InteractInstigator;            // 취소·완료가 같은 플레이어에게서 왔는지 확인용
	SetCitizenState(ETBCitizenState::Draining);
}

void ATBCitizenNPC::CancelInteract_Implementation(AActor* InteractInstigator)
{
	if (!HasAuthority() || CitizenState != ETBCitizenState::Draining || DrainInstigator.Get() != InteractInstigator)
	{
		return;
	}

	SetCitizenState(ETBCitizenState::Idle);          // 다시 도망칠 수 있는 상태로
}

void ATBCitizenNPC::Interact_Implementation(AActor* InteractInstigator)
{
	// [서버] 3인칭 몽타주 완료 시 (몽타주가 없으면 GA가 즉시 호출 → 이때는 Idle 상태로 들어옴)
	if (!HasAuthority() || !InteractInstigator || CitizenState == ETBCitizenState::Dead)
	{
		return;
	}

	if (CitizenState == ETBCitizenState::Draining && DrainInstigator.Get() != InteractInstigator)
	{
		return;                                      // 다른 플레이어의 결과 요청은 무시
	}

	if (FVector::Dist(GetActorLocation(), InteractInstigator->GetActorLocation()) > MaxDrainDistance)
	{
		SetCitizenState(ETBCitizenState::Idle);      // 실패해도 Draining에 갇히지 않게 복구
		return;
	}

	// 회복은 플레이어 쪽 HealComponent가 단일 창구 (시민은 PlayerState를 몰라도 됨)
	if (UTBHealComponent* HealComp = InteractInstigator->FindComponentByClass<UTBHealComponent>())
	{
		HealComp->ApplyHeal();
	}

	SetCitizenState(ETBCitizenState::Dead);
}

void ATBCitizenNPC::InteractLocal_Implementation(AActor* InteractInstigator)
{
	// [흡혈한 플레이어의 클라이언트] 화면 효과, 사운드 자리
}

FText ATBCitizenNPC::GetInteractPrompt_Implementation() const { return InteractPrompt; }
UAnimMontage* ATBCitizenNPC::GetInteractMontage_Implementation() const { return FeedMontage; }
UAnimMontage* ATBCitizenNPC::GetInteractFirstPersonMontage_Implementation() const { return FeedFirstPersonMontage; }
bool ATBCitizenNPC::ShouldResolveOnStart_Implementation() const { return false; }

// 상태 =====================
void ATBCitizenNPC::SetCitizenState(ETBCitizenState NewState)
{
	if (!HasAuthority() || CitizenState == NewState)
	{
		return;
	}

	const ETBCitizenState OldState = CitizenState;
	CitizenState = NewState;                         // 클라이언트로 복제 → OnRep

	// 1) 다른 시스템이 읽는 태그를 상태에서 파생 (태그를 쓰는 곳은 여기 한 곳뿐)
	//    MovementBlocked → TBCharacterBase::IsMovementBlocked가 속도 0 처리
	//    State_Dead      → 사망 GA의 ActivationRequiredTags 충족
	AbilitySystemComponent->SetLooseGameplayTagCount(TBGameplayTags::State_MovementBlocked,
		NewState == ETBCitizenState::Idle ? 0 : 1, EGameplayTagReplicationState::TagOnly);
	AbilitySystemComponent->SetLooseGameplayTagCount(TBGameplayTags::State_Dead,
		NewState == ETBCitizenState::Dead ? 1 : 0, EGameplayTagReplicationState::TagOnly);

	// 2) 보이는 처리 (서버에서는 OnRep이 자동 호출되지 않음)
	OnRep_CitizenState();

	// 3) 서버 전용 처리
	ATBCitizenAIController* CitizenAI = Cast<ATBCitizenAIController>(GetController());
	switch (NewState)
	{
	case ETBCitizenState::Draining:
		if (CitizenAI) CitizenAI->PauseForDrain();
		break;

	case ETBCitizenState::Idle:
		DrainInstigator.Reset();
		if (OldState == ETBCitizenState::Draining && CitizenAI) CitizenAI->ResumeFromDrain();
		break;

	case ETBCitizenState::Dead:
		DrainInstigator.Reset();
		if (CitizenAI) CitizenAI->StopFleeing();
		StartDeathSequence();
		break;

	default:
		break;
	}

	ForceNetUpdate();                                // 상태 변화를 다음 복제 주기까지 기다리지 않고 바로 보냄
}

void ATBCitizenNPC::OnRep_CitizenState()
{
	switch (CitizenState)
	{
	case ETBCitizenState::Draining:
		if (VictimMontage) PlayAnimMontage(VictimMontage);
		break;

	case ETBCitizenState::Dead:
		// 주의: 인자 없이 StopAnimMontage()를 부르면 "현재 몽타주" = 사망 몽타주까지 멈춤
		if (VictimMontage) StopAnimMontage(VictimMontage);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 상호작용 탐지·길막 모두 해제
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		break;

	case ETBCitizenState::Idle:
		if (VictimMontage) StopAnimMontage(VictimMontage);
		RestoreAliveState();                         // 흡혈 취소 또는 풀 재사용
		break;

	default:
		break;
	}
}

void ATBCitizenNPC::RestoreAliveState()
{
	// BP 기본값(CDO) 기준으로 충돌 복구 (ATBMonster::RestorePoolVisualState와 같은 방식)
	if (const ATBCitizenNPC* Defaults = GetClass()->GetDefaultObject<ATBCitizenNPC>())
	{
		GetCapsuleComponent()->SetCollisionEnabled(Defaults->GetCapsuleComponent()->GetCollisionEnabled());
		GetMesh()->SetCollisionEnabled(Defaults->GetMesh()->GetCollisionEnabled());
	}
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

// 사망 =====================
void ATBCitizenNPC::StartDeathSequence()
{
	float HoldTime = CorpseHoldSeconds;

	// State_Dead가 이미 붙어 있으므로 사망 GA 활성화 조건 충족
	if (AbilitySystemComponent->TryActivateAbilityByTag(TBGameplayTags::Ability_Death))
	{
		// 사망 GA가 ASC로 재생한 몽타주 → 길이만큼 더 기다림
		if (const UAnimMontage* DeathMontage = AbilitySystemComponent->GetCurrentMontage())
		{
			HoldTime += DeathMontage->GetPlayLength();
		}
	}

	// if (UTBSpawnManagerComponent::NotifyActorDied(this)) return;
	GetWorldTimerManager().SetTimer(CorpseTimer, this, &ThisClass::HandleCorpseExpired, FMath::Max(HoldTime, 0.1f), false);
}

void ATBCitizenNPC::HandleCorpseExpired()
{
	if (OnCitizenCorpseExpired.IsBound())
	{
		OnCitizenCorpseExpired.Broadcast(this);      // 풀 소속 → SpawnManager가 반환
	}
	else
	{
		Destroy();                                   // 레벨에 직접 배치한 시민은 풀이 없으므로 제거
	}
}

// 풀링 =====================
void ATBCitizenNPC::OnPoolActivated_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CorpseTimer);
	AbilitySystemComponent->CancelAllAbilities();    // 남아 있는 사망 GA 종료 → 사망 몽타주 정지

	SetCitizenState(ETBCitizenState::Idle);          // 태그 해제 + 모든 머신에서 충돌·이동 복구

	if (ATBCitizenAIController* CitizenAI = Cast<ATBCitizenAIController>(GetController()))
	{
		CitizenAI->ReactivateFromPool();
	}

	ForceNetUpdate();
}

void ATBCitizenNPC::OnPoolDeactivated_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CorpseTimer);
	GetCharacterMovement()->StopMovementImmediately();
	AbilitySystemComponent->CancelAllAbilities();
}
