#include "Character/Human/TBCitizen.h"

#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/TBSpawnManagerComponent.h"
#include "Components/TBBloodBarComponent.h"
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
#include "Character/Human/TBCitizenAppearanceData.h"

// 생성자 =====================
ATBCitizen::ATBCitizen(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AIControllerClass = ATBCitizenAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->MaxWalkSpeed = WalkSpeed;
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
}

void ATBCitizen::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBCitizen, CitizenState);
	
	DOREPLIFETIME(ATBCitizen, CurrentAppearance);
}

void ATBCitizen::BeginPlay()
{
	Super::BeginPlay();
	SetCharacterOutlineColor(0.0f, 0.0f, 1.0f);

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
	
	UpdateMoveSpeed();

	if (CurrentAppearance.IsNull())
	{
		ChooseAppearance();
	}
}

void ATBCitizen::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void ATBCitizen::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	InitializeAbilitySystem(AbilitySystemComponent.Get(), this);
}

// 상호작용 =====================
bool ATBCitizen::CanInteract_Implementation(AActor* InteractInstigator) const
{
	return CitizenState == ETBCitizenState::Idle;   // 흡혈 중이거나 죽었으면 다른 플레이어도 불가
}

void ATBCitizen::BeginInteract_Implementation(AActor* InteractInstigator)
{
	if (!HasAuthority() || CitizenState != ETBCitizenState::Idle)
	{
		return;
	}

	DrainInstigator = InteractInstigator;            // 취소·완료가 같은 플레이어에게서 왔는지 확인용
	SetCitizenState(ETBCitizenState::Drained);      // 1) BT·이동 먼저 정지 + 무적
	AlignToFeeder(InteractInstigator);               // 2) 그 다음 위치 맞춤
}

void ATBCitizen::CancelInteract_Implementation(AActor* InteractInstigator)
{
	if (!HasAuthority() || CitizenState != ETBCitizenState::Drained || DrainInstigator.Get() != InteractInstigator)
	{
		return;
	}

	SetCitizenState(ETBCitizenState::Idle);          // 다시 도망칠 수 있는 상태로 (출혈이 있었다면 다시 진행)
}

void ATBCitizen::Interact_Implementation(AActor* InteractInstigator)
{
	// [서버] 3인칭 몽타주 완료 시 (몽타주가 없으면 GA가 즉시 호출 → 이때는 Idle 상태로 들어옴)
	if (!HasAuthority() || !InteractInstigator || CitizenState == ETBCitizenState::Dead)
	{
		return;
	}

	if (CitizenState == ETBCitizenState::Drained && DrainInstigator.Get() != InteractInstigator)
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

void ATBCitizen::InteractLocal_Implementation(AActor* InteractInstigator)
{
	// [흡혈한 플레이어의 클라이언트] 화면 효과, 사운드 자리
}

FText ATBCitizen::GetInteractPrompt_Implementation() const { return InteractPrompt; }
UAnimMontage* ATBCitizen::GetInteractMontage_Implementation() const { return FeedMontage; }
UAnimMontage* ATBCitizen::GetInteractFirstPersonMontage_Implementation() const { return FeedFirstPersonMontage; }

FGameplayTag ATBCitizen::GetInteractionAbilityTag_Implementation(AActor* InteractInstigator) const
{
	// 시민은 흡혈 절차(GA_Drain)로 상호작용
	return TBGameplayTags::Ability_Interaction_Drain;
}

// 상태 =====================
void ATBCitizen::SetCitizenState(ETBCitizenState NewState)
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
		NewState == ETBCitizenState::Drained ? 1 : 0, EGameplayTagReplicationState::TagOnly);

	// 2) 보이는 처리 (서버에서는 OnRep이 자동 호출되지 않음)
	OnRep_CitizenState();

	// 3) 서버 전용 처리
	ATBCitizenAIController* CitizenAI = Cast<ATBCitizenAIController>(GetController());
	switch (NewState)
	{
	case ETBCitizenState::Drained:
		if (CitizenAI) CitizenAI->PauseForDrain();
		break;

	case ETBCitizenState::Idle:
		DrainInstigator.Reset();
		if (OldState == ETBCitizenState::Drained && CitizenAI) CitizenAI->ResumeFromDrain();
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

void ATBCitizen::OnRep_CitizenState()
{
	SetCharacterOutlineSuppressed(CitizenState == ETBCitizenState::Dead);
	switch (CitizenState)
	{
	case ETBCitizenState::Drained:
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

void ATBCitizen::RestoreAliveState()
{
	// BP 기본값(CDO) 기준으로 충돌 복구 (ATBMonster::RestorePoolVisualState와 같은 방식)
	if (const ATBCitizen* Defaults = GetClass()->GetDefaultObject<ATBCitizen>())
	{
		GetCapsuleComponent()->SetCollisionEnabled(Defaults->GetCapsuleComponent()->GetCollisionEnabled());
		GetMesh()->SetCollisionEnabled(Defaults->GetMesh()->GetCollisionEnabled());
	}
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ATBCitizen::UpdateMoveSpeed()
{
	float NewSpeed = WalkSpeed;
	if (IsInjured())
	{
		NewSpeed = InjuredWalkSpeed;
	}
	else if (bFleeing)
	{
		NewSpeed = RunSpeed;
	}
	
	GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
	ForceNetUpdate();
}

void ATBCitizen::ChooseAppearance()
{
	if (!HasAuthority() || !AppearanceCandidates)
	{
		return;
	}

	TArray<USkeletalMesh*> ValidMeshes;
	for (USkeletalMesh* CandidateMesh : AppearanceCandidates->SkeletalMesh)
	{
		if (CandidateMesh)
		{
			ValidMeshes.Add(CandidateMesh);
		}
	}

	if (ValidMeshes.IsEmpty())
	{
		return;
	}

	const int32 Index = FMath::RandRange(0, ValidMeshes.Num() - 1);
	CurrentAppearance = ValidMeshes[Index];
	ApplyAppearance();
	ForceNetUpdate();
}

void ATBCitizen::OnRep_Appearance()
{
	ApplyAppearance();
}

void ATBCitizen::ApplyAppearance()
{
	if (USkeletalMesh* SelectedMesh = CurrentAppearance.LoadSynchronous())
	{
		GetMesh()->SetSkeletalMesh(SelectedMesh);
	}
}

void ATBCitizen::StartDeathSequence()
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

void ATBCitizen::HandleCorpseExpired()
{
	// 풀 소속이면 SpawnManager가 ReturnDelay 뒤 반환, 아니면(레벨 직접 배치 등) 직접 제거
	if (!UTBSpawnManagerComponent::NotifyActorDied(this))
	{
		Destroy();
	}
}

// 풀링 =====================
void ATBCitizen::OnPoolActivated_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(CorpseTimer);
	AbilitySystemComponent->CancelAllAbilities();    // 남아 있는 사망 GA 종료 → 사망 몽타주 정지

	ResetVitals();                                   // 혈액 가득 + 출혈 제거

	ChooseAppearance();                              // 풀에서 다시 나올 때마다 새 옷

	SetCitizenState(ETBCitizenState::Idle);          // 태그 해제 + 모든 머신에서 충돌·이동 복구
}

void ATBCitizen::OnPoolReady()
{
	if (!HasAuthority()) return;
	if (ATBCitizenAIController* CitizenAI = Cast<ATBCitizenAIController>(GetController()))
	{
		CitizenAI->ReactivateFromPool();
	}
}

void ATBCitizen::OnPoolDeactivated_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}
	if (BloodBarComponent) BloodBarComponent->ClearSelectionsForPooledActor();

	GetWorldTimerManager().ClearTimer(CorpseTimer);
	if (ATBCitizenAIController* CitizenAI = Cast<ATBCitizenAIController>(GetController())) CitizenAI->StopFleeing();
	GetCharacterMovement()->StopMovementImmediately();
	AbilitySystemComponent->CancelAllAbilities();
	StopBleeding();
}

void ATBCitizen::AlignToFeeder(const AActor* Feeder)
{
	if (!HasAuthority() || !IsValid(Feeder))
	{
		return;
	}
	
	// 플레이어의 수평 정면·오른쪽 (카메라 위아래 각도는 무시)
	const FRotator FeederYawRot(0.f, Feeder->GetActorRotation().Yaw, 0.f);
	const FVector Forward = FeederYawRot.Vector();
	const FVector Right = FRotationMatrix(FeederYawRot).GetUnitAxis(EAxis::Y);

	FVector NewLocation = Feeder->GetActorLocation() + Forward * FeedAlignDistance + Right * FeedAlignSideOffset;
	NewLocation.Z = GetActorLocation().Z;            // 시민 자신의 높이 유지

	const FRotator NewRotation(0.f, FeederYawRot.Yaw + FeedAlignYawOffset, 0.f);
	SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
}

// 혈액·출혈 =====================
bool ATBCitizen::IsInjured() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Bleeding);
}

// 흡혈 등으로 무적이거나 사망한 Citizen은 공격을 거부한다.
bool ATBCitizen::CanReceiveAttack(float Damage) const
{
    return AbilitySystemComponent
        && !AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable)
        && !AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Dead);
}

// Citizen에 설정된 피해 GE를 공통 처리에 제공한다.
TSubclassOf<UGameplayEffect> ATBCitizen::GetAttackDamageEffect() const
{
    return DamageEffectClass;
}

// 피격 GE 적용 후 기존 Citizen 출혈 처리를 시작한다.
void ATBCitizen::OnAttackApplied(float Damage, AActor* DamageSource)
{
    StartBleeding();

    // 살아서 평상시 상태일 때만 (흡혈 중이거나 이번 공격으로 이미 죽었으면 무시)
    if (CitizenState == ETBCitizenState::Idle)
    {
        if (ATBCitizenAIController* AIC = Cast<ATBCitizenAIController>(GetController()))
        {
            AIC->ReportDamage(DamageSource);
        }
    }
}

void ATBCitizen::ResetVitals()
{
	if (!HasAuthority() || !CitizenAttributes)
	{
		return;
	}

	StopBleeding();                                  // State.Bleeding 제거 → 감속도 태그 이벤트로 복구
	CitizenAttributes->SetMaxBlood(InitialMaxBlood);
	CitizenAttributes->SetBlood(InitialMaxBlood);
}

void ATBCitizen::StartBleeding()
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

void ATBCitizen::StopBleeding()
{
	if (BleedEffectHandle.IsValid() && AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(BleedEffectHandle);
	}
	BleedEffectHandle.Invalidate();
}

void ATBCitizen::HandleOutOfBlood()
{
	SetCitizenState(ETBCitizenState::Dead);          // 흡혈 사망과 같은 경로: 태그·충돌 해제·사망 GA·시체 처리
}

void ATBCitizen::HandleBleedingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	UpdateMoveSpeed();
}

float ATBCitizen::PlayStartle(float& OutLookYawOffset)
{
	OutLookYawOffset = 0.f;
	if (!HasAuthority())
	{
		return 0.f;
	}

	TArray<int32> Candidates;
	for (int32 Index = 0; Index < StartleAnims.Num(); ++Index)
	{
		if (StartleAnims[Index].Montage)
		{
			Candidates.Add(Index);
		}
	}
	
	if (Candidates.IsEmpty())
	{
		return 0.f;
	}
	
	if (Candidates.Num() > 1)
	{
		Candidates.Remove(LastStartleIndex);
	}
	
	const int32 Chosen = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	LastStartleIndex = Chosen;
	
	OutLookYawOffset = StartleAnims[Chosen].LookYawOffset;
	MulticastPlayStartle(static_cast<uint8>(Chosen));
	return StartleAnims[Chosen].Montage->GetPlayLength();
}

void ATBCitizen::MulticastPlayStartle_Implementation(uint8 MontageIndex)
{
	if (StartleAnims.IsValidIndex(MontageIndex) && StartleAnims[MontageIndex].Montage)
	{
		PlayAnimMontage(StartleAnims[MontageIndex].Montage);
	}
}

void ATBCitizen::SetFaceFocus(bool bEnable)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->bOrientRotationToMovement = !bEnable;      // 평소: 이동 방향을 바라봄
	MoveComp->bUseControllerDesiredRotation = bEnable;   // 놀람 중: 컨트롤러 Focus 방향으로 회전
}

void ATBCitizen::SetFleeing(bool bNewFleeing)
{
	if (!HasAuthority() || bFleeing == bNewFleeing) return;
	bFleeing = bNewFleeing;
	UpdateMoveSpeed();
}
