#include "TBMonster.h"

#include "AIController.h"
#include "AI/AIControler/Monster/TBHunterAIController.h"
#include "AI/AIControler/Monster/TBMonsterAIController.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBMonsterAttributeSet.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Stagger.h"
#include "BrainComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TBMonsterMovementComponent.h"
#include "Components/TBSpawnManagerComponent.h"
#include "Components/TBRotationComponent.h"
#include "Components/TBBloodBarComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/GameplayAbility.h"
#include "TBGameplayTags.h"
#include "Interface/TBHitInterface.h"
#include "Interface/TBPoolInterface.h"
#include "NiagaraFunctionLibrary.h"

void ATBMonster::MulticastPlayBoomEffect_Implementation(UNiagaraSystem* Effect, FVector_NetQuantize Location)
{
	if (Effect && GetNetMode() != NM_DedicatedServer)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Effect, Location);
	}
}

ATBMonster::ATBMonster(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UTBMonsterMovementComponent>(ACharacter::CharacterMovementComponentName))
	, MonsterStatsTable(FSoftObjectPath(TEXT("/Game/_Game/Data/DT_MonsterStats.DT_MonsterStats")))
{
	
	// 몬스터가 직접 소유하는 ASC 생성
	AbilitySystemComponent = CreateDefaultSubobject<UTBAbilitySystemComponent>(TEXT("MonsterASC"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	MonsterAttributes = CreateDefaultSubobject<UTBMonsterAttributeSet>(TEXT("MonsterAttributes"));
	AbilitySystemComponent->AddAttributeSetSubobject(MonsterAttributes.Get());
	AttackRotationComponent = CreateDefaultSubobject<UTBRotationComponent>(TEXT("MonsterAttackRotation"));

	// 기본 AIController 사용 및 서버의 자동 빙의 설정
	AIControllerClass = ATBMonsterAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 이동 방향을 따라 몸체 회전
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 360.0f, 0.0f);

	// 몬스터 전신 메시의 기본 표시 및 애니메이션 모드 설정
	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->SetOnlyOwnerSee(false);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
}

void ATBMonster::BeginPlay()
{
	Super::BeginPlay();

	const bool bIsHunter = AIControllerClass && AIControllerClass->IsChildOf<ATBHunterAIController>();
	if (bIsHunter)
	{
		SetCharacterOutlineColor(0.5f, 0.0f, 1.0f);
	}
	else
	{
		SetCharacterOutlineColor(1.0f, 1.0f, 0.0f);
	}

	// 서버와 클라이언트에서 몬스터 자신을 ASC의 OwnerActor·AvatarActor로 연결
	InitializeMonsterAbilitySystem();
}

UTBMonsterMovementComponent* ATBMonster::GetMonsterMovement() const
{
	return Cast<UTBMonsterMovementComponent>(GetCharacterMovement());
}

bool ATBMonster::GetAttackDamage(const FGameplayTag& AttackId, float& OutDamage) const
{
	if (AttackId == TBGameplayTags::Ability_Combat_Melee)
	{
		OutDamage = MeleeDamage;
		return true;
	}

	return Super::GetAttackDamage(AttackId, OutDamage);
}

bool ATBMonster::ApplyMeleeDamage(const FHitResult& HitResult, float Damage)
{
	// 서버 판정 및 공격자 상태 검증
	if (!HasAuthority() || IsDead() || IsActionBlocked())
	{
		return false;
	}

	// 어빌리티에서 전달한 피해량 검증
	if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
	{
		return false;
	}

	AActor* HitActor = HitResult.GetActor();

	if (!IsValid(HitActor) || HitActor == this || !HitActor->Implements<UTBHitInterface>())
	{
		return false;
	}

	return ITBHitInterface::Execute_ReceiveAttackHit(HitActor, Damage, this, HitResult);
}

void ATBMonster::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버의 빙의 이후 ASC 연결 및 Controller 관련 참조 갱신
	InitializeMonsterAbilitySystem();
}

void ATBMonster::InitializeMonsterAbilitySystem()
{
	InitializeAbilitySystem(AbilitySystemComponent.Get(), this);
	if (!DeadTagChangedHandle.IsValid())
	{
		DeadTagChangedHandle = AbilitySystemComponent->RegisterGameplayTagEvent(TBGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ThisClass::HandleDeadTagChanged);
		if (IsDead())
		{
			HandleDeadTagChanged(TBGameplayTags::State_Dead, 1);
		}
	}
	if (!OutOfBloodHandle.IsValid())
	{
		OutOfBloodHandle = MonsterAttributes->OnOutOfBlood.AddUObject(this, &ThisClass::Die);
	}
	if (!StaggerFullHandle.IsValid())
	{
		StaggerFullHandle = MonsterAttributes->OnStaggerFull.AddUObject(this, &ThisClass::HandleStaggerFull);
	}
	if (!AbilityEndedHandle.IsValid())
	{
		AbilityEndedHandle = AbilitySystemComponent->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
	}
	InitializeGameplayData();
}

void ATBMonster::InitializeGameplayData()
{
	if (!HasAuthority() || bGameplayDataInitialized)
	{
		return;
	}

	bGameplayDataInitialized = true;
	if (const FTBMonsterStatsRow* Stats = FindMonsterStats())
	{
		ApplyMonsterStats(*Stats);
	}
	else if (InitialAttributesEffect)
	{
		FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(InitialAttributesEffect, 1.0f, Context);
		if (!Spec.IsValid())
		{
			bGameplayDataInitialized = false;
			return;
		}
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	if (IsDead())
	{
		return;
	}

	AbilitySystemComponent->AddCharacterAbilities(StartupAbilities);
}

const FTBMonsterStatsRow* ATBMonster::FindMonsterStats() const
{
	const UDataTable* Table = MonsterStatsTable.LoadSynchronous();
	if (!Table) return nullptr;

	FName RowName = MonsterStatsRow;
	if (RowName.IsNone())
	{
		FString ClassName = GetClass()->GetName();
		ClassName.RemoveFromStart(TEXT("BP_"));
		ClassName.RemoveFromEnd(TEXT("_C"));
		RowName = FName(*ClassName);
	}
	return Table->FindRow<FTBMonsterStatsRow>(RowName, GetName(), !MonsterStatsRow.IsNone());
}

void ATBMonster::ApplyMonsterStats(const FTBMonsterStatsRow& Stats)
{
	const float MaxBlood = FMath::Max(1.0f, Stats.MaxBlood);
	AbilitySystemComponent->SetNumericAttributeBase(UTBMonsterAttributeSet::GetMaxBloodAttribute(), MaxBlood);
	AbilitySystemComponent->SetNumericAttributeBase(UTBMonsterAttributeSet::GetMaxStaggerAttribute(), FMath::Max(0.0f, Stats.MaxStagger));
	AbilitySystemComponent->SetNumericAttributeBase(UTBMonsterAttributeSet::GetBloodAttribute(), MaxBlood);
	AbilitySystemComponent->SetNumericAttributeBase(UTBMonsterAttributeSet::GetStaggerAttribute(), 0.0f);
	AbilitySystemComponent->SetNumericAttributeBase(UTBMonsterAttributeSet::GetIncomingDamageAttribute(), 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = FMath::Max(0.0f, Stats.WalkSpeed);
	MeleeDamage = FMath::Max(0.0f, Stats.MeleeDamage);
	MeleeAttackRange = FMath::Max(0.0f, Stats.MeleeRange);
}

bool ATBMonster::IsDead() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Dead);
}

// 게임플레이 초기화가 끝난 살아 있는 몬스터만 피해를 받는다.
bool ATBMonster::CanReceiveAttack(float Damage) const
{
    return bGameplayDataInitialized && !bReturnedToPool && !IsDead();
}

// 몬스터 BP에 설정된 피해 GE를 공통 처리에 제공한다.
TSubclassOf<UGameplayEffect> ATBMonster::GetAttackDamageEffect() const
{
    return DamageEffectClass;
}

// 살아 있다면 공격자를 조사하고 Stun 상태가 아닐 때 Stagger를 누적한다.
void ATBMonster::OnAttackApplied(float Damage, AActor* DamageSource)
{
    if (!IsDead() && IsValid(DamageSource))
    {
        if (ATBMonsterAIController* MonsterController = Cast<ATBMonsterAIController>(GetController()))
            MonsterController->InvestigateDamageSource(DamageSource);
    }
    if (!IsDead() && AbilitySystemComponent && MonsterAttributes
        && !AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Stunned))
        MonsterAttributes->AddStagger(Damage);
}

void ATBMonster::HandleStaggerFull()
{
	if (!HasAuthority() || IsDead() || !AbilitySystemComponent || !MonsterAttributes)
	{
		return;
	}

	if (ATBMonsterAIController* MonsterController = Cast<ATBMonsterAIController>(GetController()))
	{
		MonsterController->PreserveTargetDuringStagger();
	}
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		if (UBrainComponent* Brain = AIController->GetBrainComponent(); Brain && Brain->IsRunning() && !Brain->IsPaused())
		{
			Brain->PauseLogic(TEXT("Monster staggered"));
			bPausedBrainForStagger = true;
		}
	}
	GetCharacterMovement()->StopMovementImmediately();

	TSubclassOf<UGameplayAbility> StaggerAbilityClass;
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (AbilityClass && AbilityClass->IsChildOf(UTBGameplayAbility_Monster_Stagger::StaticClass()))
		{
			StaggerAbilityClass = AbilityClass;
			break;
		}
	}
	if (!StaggerAbilityClass || !AbilitySystemComponent->TryActivateAbilityByClass(StaggerAbilityClass))
	{
		if (ATBMonsterAIController* MonsterController = Cast<ATBMonsterAIController>(GetController()))
		{
			MonsterController->RefreshTargetAfterStagger();
		}
		if (bPausedBrainForStagger)
		{
			if (AAIController* AIController = Cast<AAIController>(GetController()))
			{
				if (UBrainComponent* Brain = AIController->GetBrainComponent())
				{
					Brain->ResumeLogic(TEXT("Stagger ability failed"));
				}
			}
			bPausedBrainForStagger = false;
		}
		// 활성화 실패 시 다음 피해에서 다시 시도할 수 있도록 수치 복구
		MonsterAttributes->SetStagger(0.0f);
		UE_LOG(LogTemp, Warning, TEXT("%s: 그로기 어빌리티를 활성화하지 못했습니다."), *GetName());
	}
}

void ATBMonster::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (HasAuthority() && !bReturnedToPool && !IsDead() && MonsterAttributes
		&& IsValid(EndedData.AbilityThatEnded)
		&& EndedData.AbilityThatEnded->IsA<UTBGameplayAbility_Monster_Stagger>())
	{
		MonsterAttributes->SetStagger(0.0f);
		if (ATBMonsterAIController* MonsterController = Cast<ATBMonsterAIController>(GetController()))
		{
			MonsterController->RefreshTargetAfterStagger();
		}
		if (bPausedBrainForStagger)
		{
			if (AAIController* AIController = Cast<AAIController>(GetController()))
			{
				if (UBrainComponent* Brain = AIController->GetBrainComponent())
				{
					Brain->ResumeLogic(TEXT("Stagger ended"));
					if (const UBlackboardComponent* BlackboardComponent = AIController->GetBlackboardComponent();
						BlackboardComponent && BlackboardComponent->GetValueAsObject(TEXT("TargetActor")))
					{
						Brain->RestartLogic();
					}
				}
			}
			bPausedBrainForStagger = false;
		}
		if (ATBMonsterAIController* MonsterController = Cast<ATBMonsterAIController>(GetController()))
		{
			MonsterController->TryStartPendingInvestigation();
		}
	}
}

void ATBMonster::Die()
{
	if (!HasAuthority() || IsDead() || !AbilitySystemComponent)
	{
		return;
	}

	if (!DeathEffectClass)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: DeathEffectClass가 설정되지 않아 사망 처리에 실패했습니다."), *GetName());
		return;
	}
	const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(DeathEffectClass, 1.0f, AbilitySystemComponent->MakeEffectContext());
	if (!Spec.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("%s: 사망 GE Spec을 생성하지 못했습니다."), *GetName());
		return;
	}
	const FActiveGameplayEffectHandle AppliedHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	if (!AppliedHandle.WasSuccessfullyApplied() || !IsDead())
	{
		if (AppliedHandle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(AppliedHandle);
		}
		UE_LOG(LogTemp, Error, TEXT("%s: 사망 GE 적용에 실패했거나 State.Dead를 부여하지 않았습니다."), *GetName());
		return;
	}
	ForceNetUpdate();
}

void ATBMonster::HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0)
	{
		if (bDeathHandled) RestorePoolVisualState();
		return;
	}
	if (bDeathHandled)
	{
		return;
	}
	if (!HasAuthority())
	{
		HandleDeath();
		return;
	}
	AbilitySystemComponent->BlockAbilitiesWithTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat));
	AbilitySystemComponent->CancelAllAbilities();
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Monster died"));
		}
	}
	HandleDeath();
	if (!AbilitySystemComponent->TryActivateAbilityByTag(TBGameplayTags::Ability_Death))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 사망 어빌리티를 활성화하지 못했습니다."), *GetName());
	}
}

void ATBMonster::HandleDeath()
{
	if (bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OnMonsterDied.Broadcast(this);
	if (HasAuthority()) UTBSpawnManagerComponent::NotifyActorDied(this);
}

void ATBMonster::RestorePoolVisualState()
{
	// 부모 기본 구현 또는 자식에서 재정의한 데칼 정리 실행
	ITBPoolInterface::Execute_ClearHitImpactDecals(this);
	
	// 사망 처리 플래그와 메시·캡슐 충돌 및 이동 모드를 재사용 가능한 상태로 복구합니다.
	bDeathHandled = false;
	bPausedBrainForStagger = false;
	const ATBMonster* Defaults = GetClass()->GetDefaultObject<ATBMonster>();
	if (Defaults)
	{
		GetCapsuleComponent()->SetCollisionEnabled(Defaults->GetCapsuleComponent()->GetCollisionEnabled());
		GetMesh()->SetSimulatePhysics(false);
		GetMesh()->SetCollisionEnabled(Defaults->GetMesh()->GetCollisionEnabled());
		GetMesh()->SetRelativeTransform(Defaults->GetMesh()->GetRelativeTransform());
	}
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ATBMonster::OnPoolActivated_Implementation()
{
	if (!HasAuthority() || !bReturnedToPool || !AbilitySystemComponent || !MonsterAttributes) return;
	SetLifeSpan(0.0f);
	AbilitySystemComponent->CancelAllAbilities();
	const FTBMonsterStatsRow* Stats = FindMonsterStats();
	if (bDeathHandled || Stats)
	{
		AbilitySystemComponent->RemoveActiveEffects(FGameplayEffectQuery());
	}
	if (bDeathHandled)
	{
		AbilitySystemComponent->UnBlockAbilitiesWithTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat));
	}
	RestorePoolVisualState();
	if (Stats)
	{
		ApplyMonsterStats(*Stats);
		return;
	}
	MonsterAttributes->SetStagger(0.0f);
	MonsterAttributes->SetIncomingDamage(0.0f);
	MonsterAttributes->SetBlood(MonsterAttributes->GetMaxBlood());
}

void ATBMonster::OnPoolReady()
{
	if (!HasAuthority() || !bReturnedToPool) return;
	bReturnedToPool = false;
	if (ATBMonsterAIController* AI = Cast<ATBMonsterAIController>(GetController()))
	{
		AI->ReactivateFromPool();
	}
	else if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AIController->GetBrainComponent()) Brain->RestartLogic();
	}
}

void ATBMonster::OnPoolDeactivated_Implementation()
{
	// 풀 반환 전에 서버에서 이동과 실행 중인 Ability를 중지합니다.
	if (!HasAuthority()) return;
	bReturnedToPool = true;
	if (ATBMonsterAIController* AI = Cast<ATBMonsterAIController>(GetController()))
	{
		AI->DeactivateForPool();
	}
	else if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AIController->GetBrainComponent()) Brain->StopLogic(TEXT("Monster returned to pool"));
		AIController->StopMovement();
	}
	if (BloodBarComponent) BloodBarComponent->ClearSelectionsForPooledActor();
	GetCharacterMovement()->StopMovementImmediately();
	if (AbilitySystemComponent) AbilitySystemComponent->CancelAllAbilities();
}


void ATBMonster::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MonsterAttributes && OutOfBloodHandle.IsValid())
	{
		MonsterAttributes->OnOutOfBlood.Remove(OutOfBloodHandle);
		OutOfBloodHandle.Reset();
	}
	if (MonsterAttributes && StaggerFullHandle.IsValid())
	{
		MonsterAttributes->OnStaggerFull.Remove(StaggerFullHandle);
		StaggerFullHandle.Reset();
	}
	if (AbilitySystemComponent)
	{
		if (DeadTagChangedHandle.IsValid())
		{
			AbilitySystemComponent->UnregisterGameplayTagEvent(DeadTagChangedHandle, TBGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved);
			DeadTagChangedHandle.Reset();
		}
		if (AbilityEndedHandle.IsValid())
		{
			AbilitySystemComponent->OnAbilityEnded.Remove(AbilityEndedHandle);
			AbilityEndedHandle.Reset();
		}
		AbilitySystemComponent->RemoveCharacterAbilities();
	}
	Super::EndPlay(EndPlayReason);
}
