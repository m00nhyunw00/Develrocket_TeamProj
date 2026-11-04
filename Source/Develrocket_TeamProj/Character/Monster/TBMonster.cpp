#include "TBMonster.h"

#include "AIController.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "Character/Monster/GAS/TBMonsterAttributeSet.h"
#include "Character/Monster/GAS/TBMonsterGameplayEffects.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Net/UnrealNetwork.h"
#include "TBGameplayTags.h"
#include "Character/Monster/GAS/TBMonsterGameplayTags.h"

ATBMonster::ATBMonster(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// 몬스터가 직접 소유하는 ASC 생성
	MonsterASC = CreateDefaultSubobject<UTBAbilitySystemComponent>(TEXT("MonsterASC"));
	MonsterASC->SetIsReplicated(true);
	MonsterASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	MonsterAttributes = CreateDefaultSubobject<UTBMonsterAttributeSet>(TEXT("MonsterAttributes"));
	MonsterASC->AddAttributeSetSubobject(MonsterAttributes.Get());
	InitialAttributesEffect = UTBGameplayEffect_MonsterInitialize::StaticClass();
	DamageEffectClass = UTBGameplayEffect_MonsterDamage::StaticClass();

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

bool ATBMonster::ApplyGASDamage(float Damage, AActor* DamageSource)
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
	Spec.Data->SetSetByCallerMagnitude(TBMonsterGameplayTags::Data_Damage, Damage);
	return MonsterASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).WasSuccessfullyApplied();
}

void ATBMonster::Die()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	bIsDead = true;
	const FGameplayEffectSpecHandle Spec = MonsterASC->MakeOutgoingSpec(UTBGameplayEffect_MonsterDeath::StaticClass(), 1.0f, MonsterASC->MakeEffectContext());
	if (Spec.IsValid())
	{
		MonsterASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
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
	if (MonsterASC)
	{
		MonsterASC->RemoveCharacterAbilities();
	}
	Super::EndPlay(EndPlayReason);
}
