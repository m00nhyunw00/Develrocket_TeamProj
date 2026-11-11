#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Leap.h"

#include "AIController.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Player/TBPlayer.h"
#include "Components/CapsuleComponent.h"
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

void UTBGameplayAbility_Monster_Leap::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	ATBMonster* Monster = ActorInfo && ActorInfo->IsNetAuthority() ? Cast<ATBMonster>(ActorInfo->AvatarActor.Get()) : nullptr;
	const ATBPlayer* Player = TriggerEventData ? Cast<ATBPlayer>(TriggerEventData->Target.Get()) : nullptr;
	// 쿨다운 GE와 착지 후보가 유효한 경우에만 도약 준비
	const UGameplayEffect* Cooldown = GetCooldownGameplayEffect();
	FVector LandingLocation;
	if (!IsValid(Monster) || !IsValid(Player) || !bCommitOnActivate || !Cooldown
		|| Cooldown->DurationPolicy != EGameplayEffectDurationType::HasDuration
		|| !Cooldown->GetGrantedTags().HasTagExact(TBGameplayTags::Cooldown_MonsterLeap)
		|| !FindLandingLocation(*Monster, *Player, LandingLocation))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 실제 착지 콜백 등록 및 추격 이동 중단
	LeapingMonster = Monster;
	LeapTarget = Player;
	if (AAIController* Controller = Cast<AAIController>(Monster->GetController()))
	{
		Controller->StopMovement();
	}
	Monster->GetCharacterMovement()->StopMovementImmediately();
	Monster->LandedDelegate.AddUniqueDynamic(this, &ThisClass::HandleLanded);
	Monster->SetActorRotation(FRotator(0.0f, (LandingLocation - Monster->GetActorLocation()).Rotation().Yaw, 0.0f));

	// 화면 밖에서도 서버가 이륙 Notify를 처리하도록 메시 틱 설정 변경
	USkeletalMeshComponent* Mesh = Monster->GetMesh();
	AttackMesh = Mesh;
	PreviousTickOption = Mesh->VisibilityBasedAnimTickOption;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

bool UTBGameplayAbility_Monster_Leap::FindLandingLocation(const ATBMonster& Monster, const ATBPlayer& Player, FVector& Location) const
{
	const UCharacterMovementComponent* Movement = Monster.GetCharacterMovement();
	UWorld* World = Monster.GetWorld();
	if (!World || Monster.IsDead() || Player.IsDead() || Player.GetWorld() != World
		|| !Movement->IsMovingOnGround() || !FMath::IsFinite(FlightTime) || FlightTime <= 0.0f
		|| FVector::DistSquared(Monster.GetActorLocation(), Player.GetActorLocation()) > FMath::Square(MaxLeapRange))
	{
		return false;
	}

	const UCapsuleComponent* Capsule = Monster.GetCapsuleComponent();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(MonsterLeapLanding), false, &Monster);
	Query.AddIgnoredActor(&Player);

	// 두 캡슐 반경만큼 타깃 앞에 착지 후보 배치
	const FVector ToPlayer = Player.GetActorLocation() - Monster.GetActorLocation();
	const float StopDistance = Capsule->GetScaledCapsuleRadius() + Player.GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Target = Player.GetActorLocation() - ToPlayer.GetSafeNormal2D() * FMath::Min(StopDistance, ToPlayer.Size2D());
	FHitResult Ground;
	if (!World->LineTraceSingleByProfile(Ground, Target + FVector(0, 0, HalfHeight), Target - FVector(0, 0, HalfHeight * 3.0f), Capsule->GetCollisionProfileName(), Query)
		|| !Movement->IsWalkable(Ground))
	{
		return false;
	}

	// 지면 위에 몬스터 캡슐이 들어갈 공간 확인
	Location = Ground.ImpactPoint + FVector(0, 0, HalfHeight + 2.0f);
	return !World->OverlapBlockingTestByProfile(Location, FQuat::Identity, Capsule->GetCollisionProfileName(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), HalfHeight), Query);
}

void UTBGameplayAbility_Monster_Leap::LaunchLeap()
{
	ATBMonster* Monster = LeapingMonster.Get();
	const ATBPlayer* Player = LeapTarget.Get();
	if (!IsActive() || bLaunched || !IsValid(Monster)) return;

	FVector LandingLocation;
	if (!IsValid(Player) || !FindLandingLocation(*Monster, *Player, LandingLocation))
	{
		EndLeap(true);
		return;
	}

	// 이륙 시점의 착지 후보와 중력을 반영한 발사 속도 계산
	FVector Velocity = (LandingLocation - Monster->GetActorLocation()) / FlightTime;
	Velocity.Z -= 0.5f * Monster->GetCharacterMovement()->GetGravityZ() * FlightTime;
	if (Velocity.Z <= 0.0f)
	{
		EndLeap(true);
		return;
	}

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
	LeapTarget.Reset();
	AttackMesh.Reset();
	bLaunched = false;
	bLanded = false;
	bMontageCompleted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
