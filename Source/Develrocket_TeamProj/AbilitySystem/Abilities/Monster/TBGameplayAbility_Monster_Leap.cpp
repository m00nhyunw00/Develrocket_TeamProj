#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Leap.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/Monster/TBMonster.h"
#include "GameFramework/Character.h"
#include "Interface/TBHitInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "TBGameplayTags.h"
#include "TimerManager.h"

namespace
{
	bool IsValidLeapTarget(const AActor* Target)
	{
		const UAbilitySystemComponent* ASC = IsValid(Target) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
		return IsValid(ASC) && ASC->GetAvatarActor() == Target
			&& !ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
			&& (ASC->HasMatchingGameplayTag(TBGameplayTags::Character_Type_Player)
				|| ASC->HasMatchingGameplayTag(TBGameplayTags::Character_Type_Human));
	}
}

UTBGameplayAbility_Monster_Leap::UTBGameplayAbility_Monster_Leap()
{
	// 도약 중 전투·이동 제한 상태 부여 및 사망·경직 상태에서 발동 차단
	SetAssetTags(FGameplayTagContainer(TBGameplayTags::Ability_Combat_MonsterLeap));
	ActivationOwnedTags.AddTag(TBGameplayTags::State_Combat);
	ActivationOwnedTags.AddTag(TBGameplayTags::State_MovementBlocked);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_Stunned);
	ActivationBlockedTags.AddTag(TBGameplayTags::State_MovementBlocked);
}

bool UTBGameplayAbility_Monster_Leap::CanLeapAtTarget(const ATBMonster* Monster, const AActor* Target) const
{
	const UGameplayEffect* Cooldown = GetCooldownGameplayEffect();
	if (!bCommitOnActivate || !ThirdPersonMontage || !Cooldown
		|| Cooldown->DurationPolicy != EGameplayEffectDurationType::HasDuration
		|| !Cooldown->GetGrantedTags().HasTagExact(TBGameplayTags::Cooldown_MonsterLeap)
		|| !IsValid(Monster) || !Monster->HasAuthority() || !IsValidLeapTarget(Target)
		|| Monster->IsDead() || !Monster->GetWorld()
		|| Target->GetWorld() != Monster->GetWorld() || !Monster->GetCharacterMovement()
		|| !Monster->GetCharacterMovement()->IsMovingOnGround()
		|| !FMath::IsFinite(FlightTime) || FlightTime <= 0.0f
		|| !FMath::IsFinite(MaxLeapRange) || MaxLeapRange <= 0.0f
		|| FVector::DistSquared(Monster->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MaxLeapRange))
	{
		return false;
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MonsterLeapPath), false, Monster);
	Query.AddIgnoredActor(Target);
	return !Monster->GetWorld()->LineTraceTestByChannel(Monster->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Query);
}

void UTBGameplayAbility_Monster_Leap::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ATBMonster* Monster = ActorInfo && ActorInfo->IsNetAuthority() ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AActor* Target = TriggerEventData ? TriggerEventData->Target.Get() : nullptr;
	// 도약 시작 조건과 타깃까지의 장애물 확인
	const UGameplayEffect* Cooldown = GetCooldownGameplayEffect();
	if (!CanLeapAtTarget(Monster, Target) || !bCommitOnActivate || !Cooldown
		|| Cooldown->DurationPolicy != EGameplayEffectDurationType::HasDuration
		|| !Cooldown->GetGrantedTags().HasTagExact(TBGameplayTags::Cooldown_MonsterLeap))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 발동 시점의 플레이어 위치를 도약 목표로 고정
	LeapDestination = Target->GetActorLocation();
	LeapTarget = Target;
	// 실제 착지 콜백 등록 및 추격 이동 중단
	LeapingMonster = Monster;
	if (AAIController* Controller = Cast<AAIController>(Monster->GetController()))
	{
		Controller->StopMovement();
	}
	Monster->GetCharacterMovement()->StopMovementImmediately();
	Monster->LandedDelegate.AddUniqueDynamic(this, &ThisClass::HandleLanded);
	Monster->SetActorRotation(FRotator(0.0f, (LeapDestination - Monster->GetActorLocation()).Rotation().Yaw, 0.0f));

	// 화면 밖에서도 서버가 이륙 Notify를 처리하도록 메시 틱 설정 변경
	USkeletalMeshComponent* Mesh = Monster->GetMesh();
	AttackMesh = Mesh;
	PreviousTickOption = Mesh->VisibilityBasedAnimTickOption;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UTBGameplayAbility_Monster_Leap::LaunchLeap()
{
	ATBMonster* Monster = LeapingMonster.Get();
	if (!IsActive() || bLaunched || !IsValid(Monster)) return;

	// 고정된 목표 위치와 중력을 반영한 발사 속도 계산
	FVector Velocity = (LeapDestination - Monster->GetActorLocation()) / FlightTime;
	Velocity.Z = FMath::Max(0.0f, Velocity.Z) - 0.5f * Monster->GetCharacterMovement()->GetGravityZ() * FlightTime;

	bLaunched = true;
	Monster->SetActorRotation(FRotator(0.0f, Velocity.Rotation().Yaw, 0.0f));
	Monster->LaunchCharacter(Velocity, true, true);
	if (FMath::IsFinite(HomingTurnRate) && HomingTurnRate > 0.0f)
	{
		LastHomingUpdateTime = Monster->GetWorld()->GetTimeSeconds();
		Monster->GetWorldTimerManager().SetTimer(HomingTimer, this, &ThisClass::UpdateLeapHoming, 0.05f, true);
	}
	// 착지 이벤트가 누락돼도 BT 태스크가 종료되도록 시간 제한 설정
	Monster->GetWorldTimerManager().SetTimer(LandingTimeout, this, &ThisClass::HandleLandingTimeout, FlightTime + 1.0f, false);
}

void UTBGameplayAbility_Monster_Leap::UpdateLeapHoming()
{
	ATBMonster* Monster = LeapingMonster.Get();
	const AActor* Target = LeapTarget.Get();
	if (!IsActive() || !bLaunched || bLanded || !IsValid(Monster) || !Monster->HasAuthority()
		|| Monster->IsDead() || !IsValidLeapTarget(Target) || Target->GetWorld() != Monster->GetWorld())
	{
		if (Monster) Monster->GetWorldTimerManager().ClearTimer(HomingTimer);
		return;
	}
	UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
	if (!Movement || !Movement->IsFalling()) return;

	const double Now = Monster->GetWorld()->GetTimeSeconds();
	const float DeltaSeconds = FMath::Clamp(static_cast<float>(Now - LastHomingUpdateTime), 0.0f, 0.1f);
	LastHomingUpdateTime = Now;
	const FVector Offset = Target->GetActorLocation() - Monster->GetActorLocation();
	const float Speed = Movement->Velocity.Size2D();
	if (!FMath::IsFinite(HomingTurnRate) || HomingTurnRate <= 0.0f || !FMath::IsFinite(HomingStopDistance)
		|| Speed <= KINDA_SMALL_NUMBER || Offset.SizeSquared2D() <= FMath::Square(FMath::Max(0.0f, HomingStopDistance))) return;

	const float CurrentYaw = Movement->Velocity.Rotation().Yaw;
	const float TargetYaw = Offset.Rotation().Yaw;
	const float MaxTurn = HomingTurnRate * DeltaSeconds;
	const float NewYaw = CurrentYaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw), -MaxTurn, MaxTurn);
	const FVector NewDirection = FRotator(0.0f, NewYaw, 0.0f).Vector();
	Movement->Velocity.X = NewDirection.X * Speed;
	Movement->Velocity.Y = NewDirection.Y * Speed;
	Monster->SetActorRotation(FRotator(0.0f, NewYaw, 0.0f));
}

void UTBGameplayAbility_Monster_Leap::HandleLanded(const FHitResult& Hit)
{
	ATBMonster* Monster = LeapingMonster.Get();
	if (!IsActive() || !bLaunched || bLanded || !IsValid(Monster) || Monster->IsDead()) return;

	// 최초 착지만 처리하고 실제 착지 위치 기준으로 피해 대상 판정
	bLanded = true;
	Monster->GetWorldTimerManager().ClearTimer(HomingTimer);
	Monster->GetWorldTimerManager().ClearTimer(LandingTimeout);

	UWorld* World = Monster->GetWorld();
	const FVector Center = Monster->GetActorLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MonsterLeapDamage), false, Monster);
	for (TActorIterator<ACharacter> It(World); It && IsActive(); ++It)
	{
		ACharacter* Target = *It;
		if (!IsValidLeapTarget(Target) || !Target->GetClass()->ImplementsInterface(UTBHitInterface::StaticClass())
			|| FVector::DistSquared(Center, Target->GetActorLocation()) > FMath::Square(LandingRadius)) continue;
		// 착지 지점과 플레이어 사이 시야 차단 검사
		FHitResult Obstruction;
		if (World->LineTraceSingleByChannel(Obstruction, Center, Target->GetActorLocation(), ECC_Visibility, Query)
			&& Obstruction.GetActor() != Target) continue;

		const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target);
		if (!IsValid(TargetASC) || TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable)) continue;

		FHitResult HitResult;
		HitResult.Location = Target->GetActorLocation();
		HitResult.ImpactPoint = HitResult.Location;
		ITBHitInterface::Execute_ReceiveAttackHit(Target, FGameplayTag(), AttackDamage, Monster, HitResult);
	}

	if (IsActive() && bMontageCompleted) EndLeap(false);
}

void UTBGameplayAbility_Monster_Leap::HandleActionMontageCompleted()
{
	// 실제 착지와 몽타주 완료를 모두 기다리고 이륙이 없으면 취소
	bMontageCompleted = true;
	if (!bLaunched || bLanded) EndLeap(!bLaunched);
}

void UTBGameplayAbility_Monster_Leap::HandleLandingTimeout()
{
	EndLeap(true);
}

void UTBGameplayAbility_Monster_Leap::EndLeap(bool bCancelled)
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void UTBGameplayAbility_Monster_Leap::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 착지 구독과 타이머 해제 및 취소된 도약 이동 정리
	if (ATBMonster* Monster = LeapingMonster.Get())
	{
		Monster->LandedDelegate.RemoveDynamic(this, &ThisClass::HandleLanded);
		Monster->GetWorldTimerManager().ClearTimer(LandingTimeout);
		Monster->GetWorldTimerManager().ClearTimer(HomingTimer);
		if (bWasCancelled && bLaunched && !bLanded)
		{
			Monster->GetCharacterMovement()->StopMovementImmediately();
			Monster->GetCharacterMovement()->ClearAccumulatedForces();
		}
	}
	// 원래 메시 틱 설정과 다음 도약을 위한 실행 상태 복구
	if (USkeletalMeshComponent* Mesh = AttackMesh.Get()) Mesh->VisibilityBasedAnimTickOption = PreviousTickOption;
	LeapingMonster.Reset();
	LeapTarget.Reset();
	LastHomingUpdateTime = 0.0;
	LeapDestination = FVector::ZeroVector;
	AttackMesh.Reset();
	bLaunched = false;
	bLanded = false;
	bMontageCompleted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
