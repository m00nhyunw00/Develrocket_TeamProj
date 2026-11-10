#include "Components/TBInteractionComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interface/TBInteractable.h"
#include "TimerManager.h"

UTBInteractionComponent::UTBInteractionComponent()
{
	// 매 프레임 Tick 대신 타이머로 탐지
	PrimaryComponentTick.bCanEverTick = false;

	// Client RPC(Client_NotifyInteracted)를 쓰려면 컴포넌트가 네트워크에 등록되어 있어야 함
	SetIsReplicatedByDefault(true);
}

void UTBInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// ScanInterval마다 ScanForTarget 반복 호출
	// (모든 머신에서 타이머는 돌지만 ScanForTarget 안에서 로컬 조종 캐릭터만 통과)
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

// [로컬] 포커스 갱신 ----------------------

void UTBInteractionComponent::ScanForTarget()
{
	// 내가 조종하는 캐릭터만 탐지 (서버에 있는 다른 플레이어 캐릭터, 시뮬레이션 프록시는 제외)
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

	// 실제 화면 카메라 위치와 방향
	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDir = ViewRotation.Vector();

	AActor* BestTarget = FindBestTarget(OwnerPawn, ViewLocation, ViewDir);

	// 대상이 바뀌었을 때만 갱신 + 로그 (0.1초마다 로그 홍수 방지)
	if (BestTarget != FocusedActor.Get())
	{
		FocusedActor = BestTarget;
		
		OnFocusedActorChanged.Broadcast(BestTarget);
		
		UE_LOG(LogTemp, Log, TEXT("[Interaction] Focus: %s"), *GetNameSafe(BestTarget));
	}

	if (bDrawDebug)
	{
		DrawScanDebug(ViewLocation, ViewDir, BestTarget);
	}
}

// [서버] 검증용 재탐지 ----------------------

AActor* UTBInteractionComponent::FindTargetForServer() const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->HasAuthority())
	{
		return nullptr;
	}

	// 서버에는 클라이언트 화면 카메라가 없으므로
	// Pawn 눈 위치(BaseEyeHeight) + 조준 방향(Control Rotation, 클라이언트에서 복제됨) 사용
	FVector EyeLocation;
	FRotator EyeRotation;
	OwnerPawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	return FindBestTarget(OwnerPawn, EyeLocation, EyeRotation.Vector());
}

// [서버 → 소유 클라이언트] 로컬 반응 통지 ----------------------

void UTBInteractionComponent::Client_NotifyInteracted_Implementation(AActor* Target)
{
	// 이 함수는 상호작용한 플레이어의 클라이언트에서만 실행됨
	// Target이 null이면 클라이언트에 존재하지 않는 액터 (스폰 후 bReplicates가 꺼진 액터 등)
	if (!Target || !Target->Implements<UTBInteractable>())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Interaction] Local -> %s"), *GetNameSafe(Target));
	ITBInteractable::Execute_InteractLocal(Target, GetOwner());
}

// [공용] 판정 규칙 ----------------------

AActor* UTBInteractionComponent::FindBestTarget(APawn* OwnerPawn, const FVector& ViewLocation, const FVector& ViewDir) const
{
	UWorld* World = GetWorld();
	if (!World || !OwnerPawn)
	{
		return nullptr;
	}

	// ① 눈 위치 기준 반경 안의 후보 수집 (Visibility에 Block/Overlap 반응하는 콜리전)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TBInteractionQuery), false);
	Params.AddIgnoredActor(OwnerPawn);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(
		Overlaps, ViewLocation, FQuat::Identity, ECC_Visibility,
		FCollisionShape::MakeSphere(InteractRange), Params);

	// 시야 원뿔 기준값: cos(반각). 내적이 이 값 이상이면 원뿔 안
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

		// ② 상호작용 인터페이스 + 지금 상호작용 가능한 상태인지
		if (!Candidate->Implements<UTBInteractable>()
			|| !ITBInteractable::Execute_CanInteract(Candidate, OwnerPawn))
		{
			continue;
		}

		// ③ 시야 원뿔 안인지: 시선 방향과 대상 방향의 내적 (정면 1, 옆 0, 뒤 -1)
		const FVector TargetCenter = Candidate->GetComponentsBoundingBox().GetCenter();
		const FVector ToTarget = (TargetCenter - ViewLocation).GetSafeNormal();
		const float Dot = FVector::DotProduct(ViewDir, ToTarget);

		// 원뿔 밖이거나, 이미 찾은 대상보다 덜 정면이면 제외
		if (Dot < BestDot)
		{
			continue;
		}

		// ④ 벽에 가려지지 않았는지 (비싼 검사라 각도 검사 통과한 후보에만)
		FHitResult SightHit;
		const bool bBlocked = World->LineTraceSingleByChannel(
			SightHit, ViewLocation, TargetCenter, ECC_Visibility, Params);

		if (bBlocked && SightHit.GetActor() != Candidate)
		{
			continue;
		}

		// ⑤ 가장 정면에 가까운 대상 갱신
		BestDot = Dot;
		BestTarget = Candidate;
	}

	return BestTarget;
}

// [로컬] 디버그 ----------------------

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