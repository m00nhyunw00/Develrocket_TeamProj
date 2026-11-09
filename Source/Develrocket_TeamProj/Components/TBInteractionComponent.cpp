#include "Components/TBInteractionComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interface/TBInteractable.h"
#include "TimerManager.h"
#include "Engine/OverlapResult.h"

UTBInteractionComponent::UTBInteractionComponent()
{
	// 매 프레임 Tick 대신 타이머로 탐지
	PrimaryComponentTick.bCanEverTick = false;
}

void UTBInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// ScanInterval마다 ScanForTarget 반복 호출 (마지막 true = 반복)
	GetWorld()->GetTimerManager().SetTimer(
		ScanTimerHandle, this, &UTBInteractionComponent::ScanForTarget, ScanInterval, true);
}

void UTBInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 파괴 이후 타이머 호출 방지
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ScanTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UTBInteractionComponent::ScanForTarget()
{
	// ① 내가 조종하는 캐릭터만
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDir = ViewRotation.Vector();

	// ② 주변 반경 안의 후보 수집 (Visibility에 Block/Overlap 반응하는 것)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TBInteractionOverlap), false);
	Params.AddIgnoredActor(OwnerPawn);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(
		Overlaps, ViewLocation, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(InteractRange), Params);

	// 시야 원뿔 기준값: cos(반각)
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(ViewHalfAngle));

	AActor* BestTarget = nullptr;
	float BestDot = MinDot;

	// 한 액터에 콜리전 컴포넌트가 여러 개면 중복으로 들어오므로 걸러냄
	TSet<AActor*> Checked;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!Candidate || Checked.Contains(Candidate))
		{
			continue;
		}
		Checked.Add(Candidate);

		// ③ 문장 + 지금 상호작용 가능
		if (!Candidate->Implements<UTBInteractable>()
			|| !ITBInteractable::Execute_CanInteract(Candidate, OwnerPawn))
		{
			continue;
		}

		// ④ 시야 원뿔 안인지: 시선 방향과 대상 방향의 내적
		const FVector TargetCenter = Candidate->GetComponentsBoundingBox().GetCenter();
		const FVector ToTarget = (TargetCenter - ViewLocation).GetSafeNormal();
		const float Dot = FVector::DotProduct(ViewDir, ToTarget);

		if (Dot < BestDot)   // 원뿔 밖이거나, 이미 찾은 것보다 덜 정면
		{
			continue;
		}

		// ⑤ 벽에 가려지지 않았는지
		FHitResult SightHit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			SightHit, ViewLocation, TargetCenter, ECC_Visibility, Params);

		if (bBlocked && SightHit.GetActor() != Candidate)
		{
			continue;
		}

		// 가장 정면에 가까운 대상 갱신
		BestDot = Dot;
		BestTarget = Candidate;
	}

	// ⑥ 대상이 바뀌었을 때만 갱신 + 로그
	if (BestTarget != FocusedActor.Get())
	{
		FocusedActor = BestTarget;
		UE_LOG(LogTemp, Log, TEXT("[Interaction] Focus: %s"), *GetNameSafe(BestTarget));
	}

	if (bDrawDebug)
	{
		DrawScanDebug(ViewLocation, ViewDir, BestTarget);
	}
}

void UTBInteractionComponent::DrawScanDebug(const FVector& ViewLocation, const FVector& ViewDir, const AActor* Target) const
{
	const FColor Color = Target ? FColor::Green : FColor::Red;
	const float HalfAngleRad = FMath::DegreesToRadians(ViewHalfAngle);

	// 시야 원뿔
	DrawDebugCone(GetWorld(), ViewLocation, ViewDir, InteractRange,
		HalfAngleRad, HalfAngleRad, 16, Color, false, ScanInterval);

	// 선택된 대상까지 선
	if (Target)
	{
		DrawDebugLine(GetWorld(), ViewLocation,
			Target->GetComponentsBoundingBox().GetCenter(), FColor::Yellow, false, ScanInterval, 0, 2.0f);
	}
}
