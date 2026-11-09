#include "Components/TBInteractionComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interface/TBInteractable.h"
#include "TimerManager.h"

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
	// ① 내가 조종하는 캐릭터만 탐지 (다른 플레이어의 캐릭터 인스턴스는 무시)
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

	// ② 시선: 실제 화면에 그려지는 카메라 위치와 방향
	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * InteractRange;

	// ③ Sphere Sweep: 구를 시작점에서 끝점까지 밀어서 처음 닿는 물체 찾기
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TBInteractionTrace), false);
	Params.AddIgnoredActor(OwnerPawn);   // 내 캡슐에 맞지 않도록

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		ViewLocation,
		TraceEnd,
		FQuat::Identity,                              // 구는 회전이 의미 없음
		ECC_Visibility,
		FCollisionShape::MakeSphere(TraceRadius),
		Params);

	// ④ 맞은 액터가 문장(인터페이스)을 달았고, 지금 상호작용 가능한지
	AActor* NewTarget = nullptr;
	AActor* HitActor = Hit.GetActor();

	if (bHit && HitActor
		&& HitActor->Implements<UTBInteractable>()    // 계약 여부 검사는 U 클래스
		&& ITBInteractable::Execute_CanInteract(HitActor, OwnerPawn))   // 호출은 I 클래스의 Execute_
	{
		NewTarget = HitActor;
	}

	// ⑤ 대상이 바뀌었을 때만 갱신 + 로그
	if (NewTarget != FocusedActor.Get())
	{
		FocusedActor = NewTarget;
		UE_LOG(LogTemp, Log, TEXT("[Interaction] Focus: %s"), *GetNameSafe(NewTarget));
	}

	// ⑥ 디버그 표시
	if (bDrawDebug)
	{
		DrawScanDebug(ViewLocation, TraceEnd, Hit, NewTarget != nullptr);
	}
}

void UTBInteractionComponent::DrawScanDebug(const FVector& Start, const FVector& End, const FHitResult& Hit, bool bHasTarget) const
{
	const FColor Color = bHasTarget ? FColor::Green : FColor::Red;

	// 시선 방향 선
	DrawDebugLine(GetWorld(), Start, End, Color, false, ScanInterval);

	// 끝점의 구: 판정 굵기 확인용
	DrawDebugSphere(GetWorld(), End, TraceRadius, 12, Color, false, ScanInterval);

	// 실제로 닿은 지점
	if (Hit.bBlockingHit)
	{
		DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10.0f, FColor::Yellow, false, ScanInterval);
	}
}
