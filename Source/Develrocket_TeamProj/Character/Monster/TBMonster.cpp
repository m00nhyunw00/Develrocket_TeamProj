#include "TBMonster.h"

#include "AIController.h"
#include "AI/TBMonsterAIController.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBMonsterAttributeSet.h"
#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Stagger.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/GameplayAbility.h"
#include "TBGameplayTags.h"

ATBMonster::ATBMonster(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// 몬스터가 직접 소유하는 ASC 생성
	AbilitySystemComponent = CreateDefaultSubobject<UTBAbilitySystemComponent>(TEXT("MonsterASC"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	MonsterAttributes = CreateDefaultSubobject<UTBMonsterAttributeSet>(TEXT("MonsterAttributes"));
	AbilitySystemComponent->AddAttributeSetSubobject(MonsterAttributes.Get());

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

	// 서버와 클라이언트에서 몬스터 자신을 ASC의 OwnerActor·AvatarActor로 연결
	InitializeMonsterAbilitySystem();
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
	if (InitialAttributesEffect)
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

	// 기존 단일 근접 Ability 설정과 새 초기 Ability 목록의 중복 방지
	AbilitySystemComponent->AddCharacterAbilities(StartupAbilities);
}

bool ATBMonster::IsDead() const
{
	return AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Dead);
}

bool ATBMonster::ApplyDamage(float Damage,AActor* DamageSource)
{
	if (!HasAuthority() || !bGameplayDataInitialized || IsDead() || !FMath::IsFinite(Damage) || Damage <= 0.0f || !DamageEffectClass)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = IsValid(DamageSource) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DamageSource) : nullptr;
	UAbilitySystemComponent* SpecASC = SourceASC ? SourceASC : AbilitySystemComponent.Get();
	FGameplayEffectContextHandle Context = SpecASC->MakeEffectContext();
	if (IsValid(DamageSource))
	{
		Context.AddSourceObject(DamageSource);
		if (!SourceASC)
		{
			Context.AddInstigator(DamageSource, DamageSource);
		}
	}
	const FGameplayEffectSpecHandle Spec = SpecASC->MakeOutgoingSpec(DamageEffectClass, 1.0f, Context);
	
	if (!Spec.IsValid())
	{
		return false;
	}
	Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Damage, -Damage);
	
	const bool bApplied = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
	if (bApplied && !IsDead() && !AbilitySystemComponent->HasMatchingGameplayTag(TBGameplayTags::State_Stunned))
	{
		MonsterAttributes->AddStagger(Damage);
	}
	return bApplied;
}

void ATBMonster::HandleStaggerFull()
{
	if (!HasAuthority() || IsDead() || !AbilitySystemComponent || !MonsterAttributes)
	{
		return;
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
	if (HasAuthority() && !IsDead() && MonsterAttributes
		&& IsValid(EndedData.AbilityThatEnded)
		&& EndedData.AbilityThatEnded->IsA<UTBGameplayAbility_Monster_Stagger>())
	{
		MonsterAttributes->SetStagger(0.0f);
		if (bPausedBrainForStagger)
		{
			if (AAIController* AIController = Cast<AAIController>(GetController()))
			{
				if (UBrainComponent* Brain = AIController->GetBrainComponent())
				{
					Brain->ResumeLogic(TEXT("Stagger ended"));
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
}

void ATBMonster::RestorePoolVisualState()
{
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
	// 서버에서 사망했던 몬스터를 재사용할 때 GAS 상태와 체력을 초기화합니다.
	if (!HasAuthority() || !bDeathHandled || !AbilitySystemComponent || !MonsterAttributes) return;
	SetLifeSpan(0.0f);
	AbilitySystemComponent->CancelAllAbilities();
	// 현재 구현은 활성 Gameplay Effect를 모두 제거합니다.
	AbilitySystemComponent->RemoveActiveEffects(FGameplayEffectQuery());
	AbilitySystemComponent->UnBlockAbilitiesWithTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat));
	RestorePoolVisualState();
	MonsterAttributes->SetStagger(0.0f);
	MonsterAttributes->SetIncomingDamage(0.0f);
	MonsterAttributes->SetBlood(MonsterAttributes->GetMaxBlood());
	if (ATBMonsterAIController* AI = Cast<ATBMonsterAIController>(GetController())) AI->ReactivateFromPool();
	ForceNetUpdate();
}

void ATBMonster::OnPoolDeactivated_Implementation()
{
	// 풀 반환 전에 서버에서 이동과 실행 중인 Ability를 중지합니다.
	if (!HasAuthority()) return;
	GetCharacterMovement()->StopMovementImmediately();
	if (AbilitySystemComponent) AbilitySystemComponent->CancelAllAbilities();
}
bool ATBMonster::ReceiveAttackHit_Implementation(const FGameplayTag& DamageType, float Damage, AActor* DamageSource,
	const FHitResult& HitResult)
{
	const bool bApplied = ApplyDamage(Damage, DamageSource);
	if (bApplied && !IsDead() && IsValid(DamageSource))
	{
		if (ATBMonsterAIController* MonsterController = Cast<ATBMonsterAIController>(GetController()))
		{
			MonsterController->InvestigateDamageSource(DamageSource);
		}
	}
	return bApplied;
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
