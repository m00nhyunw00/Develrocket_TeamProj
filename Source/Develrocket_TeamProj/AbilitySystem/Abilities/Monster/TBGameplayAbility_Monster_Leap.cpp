#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Leap.h"

#include "AIController.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffect.h"
#include "TBGameplayTags.h"
#include "TimerManager.h"

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

bool UTBGameplayAbility_Monster_Leap::CanLeapAtTarget(const ATBMonster* Monster, const ATBPlayer* Player) const
{
	const UGameplayEffect* Cooldown = GetCooldownGameplayEffect();
	if (!bCommitOnActivate || !ThirdPersonMontage || !Cooldown
		|| Cooldown->DurationPolicy != EGameplayEffectDurationType::HasDuration
		|| !Cooldown->GetGrantedTags().HasTagExact(TBGameplayTags::Cooldown_MonsterLeap)
		|| !IsValid(Monster) || !Monster->HasAuthority() || !IsValid(Player)
		|| Monster->IsDead() || Player->IsDead() || !Monster->GetWorld()
		|| Player->GetWorld() != Monster->GetWorld() || !Monster->GetCharacterMovement()
		|| !Monster->GetCharacterMovement()->IsMovingOnGround()
		|| !FMath::IsFinite(FlightTime) || FlightTime <= 0.0f
		|| !FMath::IsFinite(MaxLeapRange) || MaxLeapRange <= 0.0f
		|| FVector::DistSquared(Monster->GetActorLocation(), Player->GetActorLocation()) > FMath::Square(MaxLeapRange))
	{
		return false;
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MonsterLeapPath), false, Monster);
	Query.AddIgnoredActor(Player);
	return !Monster->GetWorld()->LineTraceTestByChannel(Monster->GetActorLocation(), Player->GetActorLocation(), ECC_Visibility, Query);
}

void UTBGameplayAbility_Monster_Leap::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ATBMonster* Monster = ActorInfo && ActorInfo->IsNetAuthority() ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
	const ATBPlayer* Player = TriggerEventData ? Cast<ATBPlayer>(TriggerEventData->Target.Get()) : nullptr;
	// 도약 시작 조건과 타깃까지의 장애물 확인
	const UGameplayEffect* Cooldown = GetCooldownGameplayEffect();
	if (!CanLeapAtTarget(Monster, Player) || !bCommitOnActivate || !Cooldown
		|| Cooldown->DurationPolicy != EGameplayEffectDurationType::HasDuration
		|| !Cooldown->GetGrantedTags().HasTagExact(TBGameplayTags::Cooldown_MonsterLeap))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 발동 시점의 플레이어 위치를 도약 목표로 고정
	LeapDestination = Player->GetActorLocation();
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
	// 착지 이벤트가 누락돼도 BT 태스크가 종료되도록 시간 제한 설정
	Monster->GetWorldTimerManager().SetTimer(LandingTimeout, this, &ThisClass::HandleLandingTimeout, FlightTime + 1.0f, false);
}

void UTBGameplayAbility_Monster_Leap::HandleLanded(const FHitResult& Hit)
{
	ATBMonster* Monster = LeapingMonster.Get();
	if (!IsActive() || !bLaunched || bLanded || !IsValid(Monster) || Monster->IsDead()) return;

	// 최초 착지만 처리하고 실제 착지 위치 기준으로 피해 대상 판정
	bLanded = true;
	Monster->GetWorldTimerManager().ClearTimer(LandingTimeout);

	UWorld* World = Monster->GetWorld();
	const FVector Center = Monster->GetActorLocation();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MonsterLeapDamage), false, Monster);
	for (TActorIterator<ATBPlayer> It(World); It && IsActive(); ++It)
	{
		ATBPlayer* Player = *It;
		if (!IsValid(Player) || Player->IsDead() || FVector::DistSquared(Center, Player->GetActorLocation()) > FMath::Square(LandingRadius)) continue;
		// 착지 지점과 플레이어 사이 시야 차단 검사
		FHitResult Obstruction;
		if (World->LineTraceSingleByChannel(Obstruction, Center, Player->GetActorLocation(), ECC_Visibility, Query)
			&& Obstruction.GetActor() != Player) continue;

		Player->ApplyDamage(AttackDamage, Monster);
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
		if (bWasCancelled && bLaunched && !bLanded)
		{
			Monster->GetCharacterMovement()->StopMovementImmediately();
			Monster->GetCharacterMovement()->ClearAccumulatedForces();
		}
	}
	// 원래 메시 틱 설정과 다음 도약을 위한 실행 상태 복구
	if (USkeletalMeshComponent* Mesh = AttackMesh.Get()) Mesh->VisibilityBasedAnimTickOption = PreviousTickOption;
	LeapingMonster.Reset();
	LeapDestination = FVector::ZeroVector;
	AttackMesh.Reset();
	bLaunched = false;
	bLanded = false;
	bMontageCompleted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
