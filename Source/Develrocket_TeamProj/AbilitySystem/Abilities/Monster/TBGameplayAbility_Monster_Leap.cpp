#include "AbilitySystem/Abilities/Monster/TBGameplayAbility_Monster_Leap.h"

#include "AIController.h"
#include "AI/TBAIMovementVisibilitySubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Character/Monster/TBMonster.h"
#include "Character/Human/TBCitizen.h"
#include "Character/Player/TBPlayer.h"
#include "GameFramework/Character.h"
#include "Interface/TBHitInterface.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TBMonsterMovementComponent.h"
#include "Components/TBRotationComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TBGameplayTags.h"
#include "TimerManager.h"

namespace
{
	// 도약은 살아 있는 플레이어와 Citizen 및 각 자식 BP를 대상으로 한다.
	bool IsValidLeapTarget(const AActor* Target)
	{
		const UAbilitySystemComponent* ASC = IsValid(Target) ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target) : nullptr;
		return IsValid(ASC) && ASC->GetAvatarActor() == Target
			&& !ASC->HasMatchingGameplayTag(TBGameplayTags::State_Dead)
			&& (Target->IsA<ATBPlayer>() || Target->IsA<ATBCitizen>());
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
	if (!bCommitOnActivate || !ThirdPersonMontage || !HasConfiguredSkillCooldown()
		|| !IsValid(Monster) || !Monster->HasAuthority() || !IsValidLeapTarget(Target)
		|| Monster->IsDead() || !Monster->GetWorld()
		|| Target->GetWorld() != Monster->GetWorld() || !Monster->GetCharacterMovement()
		|| !Monster->GetCharacterMovement()->IsMovingOnGround()
		|| !FMath::IsFinite(FlightTime) || FlightTime <= 0.0f
		|| !FMath::IsFinite(MaxActiveDuration) || MaxActiveDuration <= 0.0f
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
	if (!CanLeapAtTarget(Monster, Target))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	LeapTarget = Target;
	// 실제 착지 콜백 등록 및 추격 이동 중단
	LeapingMonster = Monster;
	if (AAIController* Controller = Cast<AAIController>(Monster->GetController()))
	{
		Controller->StopMovement();
	}
	UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Monster->LandedDelegate.AddUniqueDynamic(this, &ThisClass::HandleLanded);

	if (UTBRotationComponent* Rotation = Monster->GetAttackRotationComponent(); IsValid(Rotation))
	{
		// 도약 중에는 회전 컴포넌트에서만 몸체 방향 제어
		bUsingAttackRotation = true;
		bPreviousOrientRotationToMovement = Movement->bOrientRotationToMovement;
		bPreviousUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
		bPreviousUseControllerRotationYaw = Monster->bUseControllerRotationYaw;
		Movement->bOrientRotationToMovement = false;
		Movement->bUseControllerDesiredRotation = false;
		Monster->bUseControllerRotationYaw = false;
		Rotation->ToggleShouldRotate(true);
	}

	// 화면 밖에서도 서버가 이륙 Notify를 처리하도록 메시 틱 설정 변경
	USkeletalMeshComponent* Mesh = Monster->GetMesh();
	AttackMesh = Mesh;
	PreviousTickOption = Mesh->VisibilityBasedAnimTickOption;
	Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	Monster->GetWorldTimerManager().SetTimer(AbilityTimeout, this, &ThisClass::HandleAbilityTimeout, MaxActiveDuration, false);
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UTBGameplayAbility_Monster_Leap::LaunchLeap()
{
	ATBMonster* Monster = LeapingMonster.Get();
	if (!IsActive() || bLaunched || !IsValid(Monster)) return;
	const AActor* Target = LeapTarget.Get();
	if (!IsValidLeapTarget(Target) || Target->GetWorld() != Monster->GetWorld())
	{
		EndLeap(true);
		return;
	}
	// 대상과의 거리는 속도에, 몬스터의 전방은 수평 이동 방향에 사용
	const FVector TargetOffset = Target->GetActorLocation() - Monster->GetActorLocation();
	LeapHorizontalSpeed = TargetOffset.Size2D() / FlightTime;
	FVector Velocity = Monster->GetActorForwardVector().GetSafeNormal2D() * LeapHorizontalSpeed;
	Velocity.Z = FMath::Max(0.0, TargetOffset.Z / FlightTime) - 0.5 * Monster->GetCharacterMovement()->GetGravityZ() * FlightTime;
	bLaunched = true;
	if (auto* Movement = Monster->GetMonsterMovement())
	{
		Movement->SetLeapMovementActive(true);
		if (auto* Visibility = Monster->GetWorld()->GetSubsystem<UTBAIMovementVisibilitySubsystem>())
		{
			Visibility->RestoreMovementTick(*Monster);
		}
	}
	Monster->OnCharacterMovementUpdated.AddUniqueDynamic(this, &ThisClass::UpdateLeapMovement);
	Monster->GetCapsuleComponent()->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleLeapHit);
	if (USkeletalMeshComponent* Mesh = AttackMesh.Get())
	{
		Mesh->OnComponentHit.AddUniqueDynamic(this, &ThisClass::HandleLeapHit);
		Mesh->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleLeapOverlap);
	}
	Monster->LaunchCharacter(Velocity, true, true);
	if (!IsActive()) return;
	Monster->GetWorldTimerManager().SetTimer(ContactTimer, this, &ThisClass::CheckLeapContacts, 0.05f, true);
}

void UTBGameplayAbility_Monster_Leap::UpdateLeapMovement(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	ATBMonster* Monster = LeapingMonster.Get();
	if (!IsActive() || !bLaunched || bLanded || !IsValid(Monster) || !Monster->HasAuthority()
		|| Monster->IsDead())
		return;
	UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
	if (!Movement || !Movement->IsFalling()) return;

	double Speed = LeapHorizontalSpeed;
	const AActor* Target = LeapTarget.Get();
	if (HomingStopDistance > 0.0f && IsValidLeapTarget(Target) && Target->GetWorld() == Monster->GetWorld())
	{
		const double Distance = FVector::Dist2D(Monster->GetActorLocation(), Target->GetActorLocation());
		Speed *= FMath::Clamp(Distance / HomingStopDistance, 0.0, 1.0);
	}
	const FVector HorizontalVelocity = Monster->GetActorForwardVector().GetSafeNormal2D() * Speed;
	Movement->Velocity.X = HorizontalVelocity.X;
	Movement->Velocity.Y = HorizontalVelocity.Y;
}

void UTBGameplayAbility_Monster_Leap::CheckLeapContacts()
{
	ATBMonster* Monster = LeapingMonster.Get();
	USkeletalMeshComponent* Mesh = AttackMesh.Get();
	UPhysicsAsset* PhysicsAsset = Mesh ? Mesh->GetPhysicsAsset() : nullptr;
	if (!IsActive() || !bLaunched || bLanded || !IsValid(Monster) || !Monster->HasAuthority()
		|| Monster->IsDead()) return;

	// 도약을 시작할 때 선택한 대상만 검사한다.
	ACharacter* Target = Cast<ACharacter>(const_cast<AActor*>(LeapTarget.Get()));
	if (!IsValidLeapTarget(Target) || DamagedActors.Contains(Target)) return;
	UCapsuleComponent* TargetCapsule = Target->GetCapsuleComponent();
	if (!IsValid(TargetCapsule) || TargetCapsule->GetCollisionEnabled() == ECollisionEnabled::NoCollision) return;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(TargetCapsule->GetScaledCapsuleRadius(), TargetCapsule->GetScaledCapsuleHalfHeight());
	if (FBodyInstance* Body = Monster->GetCapsuleComponent()->GetBodyInstance();
		Body && Body->IsValidBodyInstance()
		&& Body->OverlapTest(TargetCapsule->GetComponentLocation(), TargetCapsule->GetComponentQuat(), Shape))
	{
		ApplyLeapContactDamage(Target, FHitResult());
		return;
	}
	if (!IsValid(Mesh) || !IsValid(PhysicsAsset)) return;
	for (const USkeletalBodySetup* Setup : PhysicsAsset->SkeletalBodySetups)
	{
		if (!IsValid(Setup)) continue;
		FBodyInstance* Body = Mesh->GetBodyInstance(Setup->BoneName, false);
		if (!Body || !Body->IsValidBodyInstance() || Body->GetBodySetup() != Setup) continue;
		if (Body->OverlapTest(TargetCapsule->GetComponentLocation(), TargetCapsule->GetComponentQuat(), Shape))
		{
			FHitResult Hit;
			Hit.Location = TargetCapsule->GetComponentLocation();
			Hit.ImpactPoint = Hit.Location;
			ApplyLeapContactDamage(Target, Hit);
			break;
		}
	}
}

void UTBGameplayAbility_Monster_Leap::HandleLeapHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	ApplyLeapContactDamage(OtherActor, Hit);
}

void UTBGameplayAbility_Monster_Leap::HandleLeapOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	ApplyLeapContactDamage(OtherActor, SweepResult);
}

void UTBGameplayAbility_Monster_Leap::ApplyLeapContactDamage(AActor* OtherActor, const FHitResult& Hit)
{
	ATBMonster* Monster = LeapingMonster.Get();
	if (!IsActive() || !bLaunched || bLanded || !IsValid(Monster) || !Monster->HasAuthority()
		|| Monster->IsDead() || OtherActor != LeapTarget.Get() || !IsValidLeapTarget(OtherActor) || DamagedActors.Contains(OtherActor)
		|| !OtherActor->GetClass()->ImplementsInterface(UTBHitInterface::StaticClass())) return;

	const UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OtherActor);
	if (!IsValid(TargetASC) || TargetASC->HasMatchingGameplayTag(TBGameplayTags::State_Invulnerable)) return;

	DamagedActors.Add(OtherActor);
	FHitResult ContactHit = Hit;
	if (!ContactHit.bBlockingHit && ContactHit.ImpactPoint.IsNearlyZero())
	{
		ContactHit.Location = OtherActor->GetActorLocation();
		ContactHit.ImpactPoint = ContactHit.Location;
	}
	ITBHitInterface::Execute_ReceiveAttackHit(OtherActor, FGameplayTag(), AttackDamage, Monster, ContactHit);
}

void UTBGameplayAbility_Monster_Leap::HandleLanded(const FHitResult& Hit)
{
	ATBMonster* Monster = LeapingMonster.Get();
	if (!IsActive() || !bLaunched || bLanded || !IsValid(Monster) || Monster->IsDead()) return;
	CheckLeapContacts();
	if (!IsActive()) return;

	// 최초 착지만 처리하고 실제 착지 위치 기준으로 피해 대상 판정
	bLanded = true;
	StopLeapTracking();
	Monster->GetWorldTimerManager().ClearTimer(ContactTimer);

	if (IsActive() && bMontageCompleted) EndLeap(false);
}

void UTBGameplayAbility_Monster_Leap::HandleActionMontageCompleted()
{
	// 실제 착지와 몽타주 완료를 모두 기다리고 이륙이 없으면 취소
	bMontageCompleted = true;
	if (!bLaunched || bLanded) EndLeap(!bLaunched);
}

void UTBGameplayAbility_Monster_Leap::HandleAbilityTimeout()
{
	EndLeap(true);
}

void UTBGameplayAbility_Monster_Leap::StopLeapTracking()
{
	if (ATBMonster* Monster = LeapingMonster.Get())
	{
		Monster->OnCharacterMovementUpdated.RemoveDynamic(this, &ThisClass::UpdateLeapMovement);
		Monster->GetCapsuleComponent()->OnComponentHit.RemoveDynamic(this, &ThisClass::HandleLeapHit);
		if (auto* Movement = Monster->GetMonsterMovement())
		{
			Movement->SetLeapMovementActive(false);
		}
	}
}

void UTBGameplayAbility_Monster_Leap::EndLeap(bool bCancelled)
{
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled);
}

void UTBGameplayAbility_Monster_Leap::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 착지 구독과 타이머 해제 및 취소된 도약 이동 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AbilityTimeout);
		World->GetTimerManager().ClearTimer(ContactTimer);
	}
	if (USkeletalMeshComponent* Mesh = AttackMesh.Get())
	{
		Mesh->OnComponentHit.RemoveDynamic(this, &ThisClass::HandleLeapHit);
		Mesh->OnComponentBeginOverlap.RemoveDynamic(this, &ThisClass::HandleLeapOverlap);
		Mesh->VisibilityBasedAnimTickOption = PreviousTickOption;
	}
	if (ATBMonster* Monster = LeapingMonster.Get())
	{
		StopLeapTracking();
		Monster->LandedDelegate.RemoveDynamic(this, &ThisClass::HandleLanded);
		if (bUsingAttackRotation)
		{
			if (UTBRotationComponent* Rotation = Monster->GetAttackRotationComponent(); IsValid(Rotation))
			{
				Rotation->ToggleShouldRotate(false);
			}
			UCharacterMovementComponent* Movement = Monster->GetCharacterMovement();
			Movement->bOrientRotationToMovement = bPreviousOrientRotationToMovement;
			Movement->bUseControllerDesiredRotation = bPreviousUseControllerDesiredRotation;
			Monster->bUseControllerRotationYaw = bPreviousUseControllerRotationYaw;
		}
		if (bWasCancelled && bLaunched && !bLanded)
		{
			Monster->GetCharacterMovement()->StopMovementImmediately();
			Monster->GetCharacterMovement()->ClearAccumulatedForces();
		}
	}
	// 원래 메시 틱 설정과 다음 도약을 위한 실행 상태 복구
	LeapingMonster.Reset();
	LeapTarget.Reset();
	LeapHorizontalSpeed = 0.f;
	AttackMesh.Reset();
	DamagedActors.Reset();
	bLaunched = false;
	bLanded = false;
	bMontageCompleted = false;
	bUsingAttackRotation = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
