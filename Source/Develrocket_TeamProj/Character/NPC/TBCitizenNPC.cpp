#include "Character/NPC/TBCitizenNPC.h"

#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/TBSpawnManagerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "AI/AIControler/NPC/TBCitizenAIController.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBCitizenAttributeSet.h"
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

	// 상호작용 탐지(ECC_Visibility 오버랩·트레이스)에 잡히도록 캡슐만 Visibility Block
	// (TBLoreNPC와 같은 규칙. 메시는 그대로 두어 트레이스 대상은 캡슐 하나로 유지)
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	
	InteractPrompt = FText::FromString(TEXT("흡혈하기"));

	AbilitySystemComponent = CreateDefaultSubobject<UTBAbilitySystemComponent>(TEXT("CitizenASC"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// 시민 혈액(체력) Attribute — 액터의 서브오브젝트라서 ASC 초기화 시 자동 등록
	CitizenAttributes = CreateDefaultSubobject<UTBCitizenAttributeSet>(TEXT("CitizenAttributes"));

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

	if (!HasAuthority())
	{
		return;
	}

	// 사망 GA 부여 (AddCharacterAbilities가 권한·중복 검사를 함)
	if (DeathAbilityClass)
	{
		AbilitySystemComponent->AddCharacterAbilities({ DeathAbilityClass });
	}

	// 혈액 0 → 사망, 출혈 태그 → 부상 감속
	if (!OutOfBloodHandle.IsValid())
	{
		OutOfBloodHandle = CitizenAttributes->OnOutOfBlood.AddUObject(this, &ThisClass::HandleOutOfBlood);
	}
	if (!BleedingTagHandle.IsValid())
	{
		BleedingTagHandle = AbilitySystemComponent->RegisterGameplayTagEvent(TBGameplayTags::State_Bleeding,
			EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleBleedingTagChanged);
	}

	ResetVitals();
}

void ATBCitizenNPC::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CitizenAttributes && OutOfBloodHandle.IsValid())
	{
		CitizenAttributes->OnOutOfBlood.Remove(OutOfBloodHandle);
		OutOfBloodHandle.Reset();
	}
	if (AbilitySystemComponent && BleedingTagHandle.IsValid())
	{
		AbilitySystemComponent->UnregisterGameplayTagEvent(BleedingTagHandle, TBGameplayTags::State_Bleeding, EGameplayTagEventType::NewOrRemoved);
		BleedingTagHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
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
	SetCitizenState(ETBCitizenState::Draining);      // 1) BT·이동 먼저 정지 + 무적
	AlignToFeeder(InteractInstigator);               // 2) 그 다음 위치 맞춤
}

void ATBCitizenNPC::CancelInteract_Implementation(AActor* InteractInstigator)
{
	if (!HasAuthority() || CitizenState != ETBCitizenState::Draining || DrainInstigator.Get() != InteractInstigator)
	{
		return;
	}

	SetCitizenState(ETBCitizenState::Idle);          // 다시 도망칠 수 있는 상태로 (출혈이 있었다면 다시 진행)
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

	// 1) 다른 시스템이 읽는 태그를 상태에서 파생 (상태 태그를 쓰는 곳은 여기 한 곳뿐)
	//    MovementBlocked → TBCharacterBase::IsMovementBlocked가 속도 0 처리
	//    State_Dead      → 사망 GA의 ActivationRequiredTags 충족, 피해·몬스터 대상 제외
	//    Invulnerable    → 흡혈 중 피해 차단(AttributeSet), 몬스터 대상 제외
	AbilitySystemComponent->SetLooseGameplayTagCount(TBGameplayTags::State_MovementBlocked,
		NewState == ETBCitizenState::Idle ? 0 : 1, EGameplayTagReplicationState::TagOnly);
	AbilitySystemComponent->SetLooseGameplayTagCount(TBGameplayTags::State_Dead,
		NewState == ETBCitizenState::Dead ? 1 : 0, EGameplayTagReplicationState::TagOnly);
	AbilitySystemComponent->SetLooseGameplayTagCount(TBGameplayTags::State_Invulnerable,
		NewState == ETBCitizenState::Draining ? 1 : 0, EGameplayTagReplicationState::TagOnly);

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
		StopBleeding();                              // 시체에는 출혈·부상 상태가 필요 없음
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

	// 시체를 HoldTime 동안 유지한 뒤 처리 (풀 반환 시점도 시민이 결정)
	GetWorldTimerManager().SetTimer(CorpseTimer, this, &ThisClass::HandleCorpseExpired, FMath::Max(HoldTime, 0.1f), false);
}

void ATBCitizenNPC::HandleCorpseExpired()
{
	// 풀 소속이면 SpawnManager가 ReturnDelay 뒤 반환, 아니면(레벨 직접 배치 등) 직접 제거
	if (!UTBSpawnManagerComponent::NotifyActorDied(this))
	{
		Destroy();
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

	ResetVitals();                                   // 혈액 가득 + 출혈 제거
	
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
	StopBleeding();
}

void ATBCitizenNPC::AlignToFeeder(const AActor* Feeder)
{
	if (!HasAuthority() || !IsValid(Feeder))
	{
		return;
	}

	// 플레이어의 수평 정면 (카메라 위아래 각도는 무시)
	const float FeederYaw = Feeder->GetActorRotation().Yaw;
	const FVector Forward = FRotator(0.f, FeederYaw, 0.f).Vector();

	FVector NewLocation = Feeder->GetActorLocation() + Forward * FeedAlignDistance;
	NewLocation.Z = GetActorLocation().Z;            // 시민 자신의 높이 유지

	const FRotator NewRotation(0.f, FeederYaw + FeedAlignYawOffset, 0.f);
	SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

// 혈액·출혈 =====================
bool ATBCitizenNPC::IsInjured() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Bleeding);
}

bool ATBCitizenNPC::ReceiveAttackHit_Implementation(const FGameplayTag& DamageType, float Damage, AActor* DamageSource, const FHitResult& HitResult)
{
	// 무적(흡혈 중)·사망이면 "피해 없음"으로 응답
	// (Instant GE는 AttributeSet에서 실행이 거부돼도 적용 성공으로 보일 수 있어서 여기서 먼저 검사)
	if (!HasAuthority() || !DamageEffectClass || !FMath::IsFinite(Damage) || Damage <= 0.f
		|| AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable)
		|| AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
	{
		return false;
	}

	FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
	if (IsValid(DamageSource))
	{
		Context.AddSourceObject(DamageSource);
	}

	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Damage, -Damage);   // 몬스터 ApplyDamage와 같은 부호 규칙

	if (!AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied())
	{
		return false;
	}

	StartBleeding();                                 // 첫 피격이면 출혈 시작 (이미 출혈 중·이번에 사망했으면 무시)
	return true;
}

void ATBCitizenNPC::ResetVitals()
{
	if (!HasAuthority() || !CitizenAttributes)
	{
		return;
	}

	StopBleeding();                                  // State.Bleeding 제거 → 감속도 태그 이벤트로 복구
	CitizenAttributes->SetMaxBlood(InitialMaxBlood);
	CitizenAttributes->SetBlood(InitialMaxBlood);
}

void ATBCitizenNPC::StartBleeding()
{
	if (!HasAuthority() || !BleedEffectClass || BleedDamagePerSecond <= 0.f || BleedEffectHandle.IsValid()
		|| AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
	{
		return;
	}

	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
		BleedEffectClass, 1.f, AbilitySystemComponent->MakeEffectContext());
	if (!Spec.IsValid())
	{
		return;
	}
	Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Damage, -BleedDamagePerSecond);

	// 무적 중에는 AttributeSet이 틱만 거부하므로 GE와 State.Bleeding 태그는 유지됨
	BleedEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
}

void ATBCitizenNPC::StopBleeding()
{
	if (BleedEffectHandle.IsValid() && AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(BleedEffectHandle);
	}
	BleedEffectHandle.Invalidate();
}

void ATBCitizenNPC::HandleOutOfBlood()
{
	SetCitizenState(ETBCitizenState::Dead);          // 흡혈 사망과 같은 경로: 태그·충돌 해제·사망 GA·시체 처리
}

void ATBCitizenNPC::HandleBleedingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// 부상 감속은 출혈 태그 기준, 해제 시 BP 기본값(CDO)으로 복구
	if (const ATBCitizenNPC* Defaults = GetClass()->GetDefaultObject<ATBCitizenNPC>())
	{
		GetCharacterMovement()->MaxWalkSpeed = NewCount > 0
			? InjuredWalkSpeed
			: Defaults->GetCharacterMovement()->MaxWalkSpeed;
	}
	ForceNetUpdate();                                // 부상 애니메이션·속도가 바로 보이도록
}