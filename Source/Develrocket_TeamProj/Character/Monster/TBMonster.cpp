#include "TBMonster.h"

#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "Character/Monster/GAS/TBMonsterAttributeSet.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Net/UnrealNetwork.h"
#include "TBGameplayTags.h"

ATBMonster::ATBMonster(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// 몬스터가 직접 소유하는 ASC 생성
	MonsterASC = CreateDefaultSubobject<UTBAbilitySystemComponent>(TEXT("MonsterASC"));
	MonsterASC->SetIsReplicated(true);
	MonsterASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	MonsterAttributes = CreateDefaultSubobject<UTBMonsterAttributeSet>(TEXT("MonsterAttributes"));
	MonsterASC->AddAttributeSetSubobject(MonsterAttributes.Get());

	// 기본 AIController 사용 및 서버의 자동 빙의 설정
	AIControllerClass = AAIController::StaticClass();
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
	InitializeAbilitySystem(MonsterASC.Get(), this);
	if (!OutOfHealthHandle.IsValid())
	{
		OutOfHealthHandle = MonsterAttributes->OnOutOfHealth.AddUObject(this, &ThisClass::Die);
	}
	if (!StaggerFullHandle.IsValid())
	{
		StaggerFullHandle = MonsterAttributes->OnStaggerFull.AddUObject(this, &ThisClass::HandleStaggerFull);
	}
	if (!AbilityEndedHandle.IsValid())
	{
		AbilityEndedHandle = MonsterASC->OnAbilityEnded.AddUObject(this, &ThisClass::HandleAbilityEnded);
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
		FGameplayEffectContextHandle Context = MonsterASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = MonsterASC->MakeOutgoingSpec(InitialAttributesEffect, 1.0f, Context);
		if (!Spec.IsValid())
		{
			bGameplayDataInitialized = false;
			return;
		}
		MonsterASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	}

	if (bIsDead)
	{
		return;
	}

	// 기존 단일 근접 Ability 설정과 새 초기 Ability 목록의 중복 방지
	TArray<TSubclassOf<UGameplayAbility>> Abilities = StartupAbilities;
	if (MeleeAbilityClass)
	{
		Abilities.AddUnique(MeleeAbilityClass);
	}
	if (StaggerAbilityClass)
	{
		Abilities.AddUnique(StaggerAbilityClass);
	}
	if (DeathAbilityClass)
	{
		Abilities.AddUnique(DeathAbilityClass);
	}
	MonsterASC->AddCharacterAbilities(Abilities);
}

float ATBMonster::GetHealth() const
{
	return MonsterAttributes ? MonsterAttributes->GetHealth() : 0.0f;
}

float ATBMonster::GetMaxHealth() const
{
	return MonsterAttributes ? MonsterAttributes->GetMaxHealth() : 0.0f;
}

bool ATBMonster::ApplyGASDamage(float Damage,AActor* DamageSource)
{
	if (!HasAuthority() || !bGameplayDataInitialized || bIsDead || !FMath::IsFinite(Damage) || Damage <= 0.0f || !DamageEffectClass)
	{
		return false;
	}

	UAbilitySystemComponent* SourceASC = IsValid(DamageSource) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(DamageSource) : nullptr;
	UAbilitySystemComponent* SpecASC = SourceASC ? SourceASC : MonsterASC.Get();
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
	Spec.Data->SetSetByCallerMagnitude(TBGameplayTags::Data_Damage, Damage);
	const bool bApplied = MonsterASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
	if (bApplied && !bIsDead && !MonsterASC->HasMatchingGameplayTag(TBGameplayTags::State_Stunned))
	{
		MonsterAttributes->AddStagger(Damage);
	}
	return bApplied;
}

void ATBMonster::HandleStaggerFull()
{
	if (!HasAuthority() || bIsDead || !MonsterASC || !MonsterAttributes)
	{
		return;
	}

	if (!StaggerAbilityClass || !MonsterASC->TryActivateAbilityByClass(StaggerAbilityClass))
	{
		// 활성화 실패 시 다음 피해에서 다시 시도할 수 있도록 수치 복구
		MonsterAttributes->SetStagger(0.0f);
		UE_LOG(LogTemp, Warning, TEXT("%s: 그로기 어빌리티를 활성화하지 못했습니다."), *GetName());
	}
}

void ATBMonster::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (HasAuthority() && !bIsDead && MonsterAttributes && StaggerAbilityClass
		&& EndedData.AbilityThatEnded && EndedData.AbilityThatEnded->GetClass() == StaggerAbilityClass)
	{
		MonsterAttributes->SetStagger(0.0f);
	}
}

void ATBMonster::Die()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	if (DeathEffectClass)
	{
		const FGameplayEffectSpecHandle Spec = MonsterASC->MakeOutgoingSpec(DeathEffectClass, 1.0f, MonsterASC->MakeEffectContext());
		if (Spec.IsValid())
		{
			MonsterASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}
	// 사망 GE가 비어 있거나 태그를 부여하지 않아도 서버의 사망 판정 유지
	if (!MonsterASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead))
	{
		MonsterASC->AddLooseGameplayTag(TBGameplayTags::State_Dead);
	}
	MonsterASC->BlockAbilitiesWithTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat));
	MonsterASC->CancelAllAbilities();
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();
		if (UBrainComponent* Brain = AIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Monster died"));
		}
	}
	HandleDeath();
	if (DeathAbilityClass && !MonsterASC->TryActivateAbilityByClass(DeathAbilityClass))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 사망 어빌리티를 활성화하지 못했습니다."), *GetName());
	}
	ForceNetUpdate();
}

void ATBMonster::OnRep_IsDead()
{
	if (bIsDead)
	{
		HandleDeath();
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
	ReceiveDeath();
	OnMonsterDied.Broadcast(this);
}

void ATBMonster::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ATBMonster, bIsDead);
}

void ATBMonster::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MonsterAttributes && OutOfHealthHandle.IsValid())
	{
		MonsterAttributes->OnOutOfHealth.Remove(OutOfHealthHandle);
		OutOfHealthHandle.Reset();
	}
	if (MonsterAttributes && StaggerFullHandle.IsValid())
	{
		MonsterAttributes->OnStaggerFull.Remove(StaggerFullHandle);
		StaggerFullHandle.Reset();
	}
	if (MonsterASC)
	{
		if (AbilityEndedHandle.IsValid())
		{
			MonsterASC->OnAbilityEnded.Remove(AbilityEndedHandle);
			AbilityEndedHandle.Reset();
		}
		MonsterASC->RemoveCharacterAbilities();
	}
	Super::EndPlay(EndPlayReason);
}
