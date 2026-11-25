#include "Components/TBMeleeTraceComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UTBMeleeTraceComponent::UTBMeleeTraceComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

bool UTBMeleeTraceComponent::PrepareAttack(UPrimitiveComponent* InMesh)
{
    AActor* OwnerActor = GetOwner();
    UWorld* World = GetWorld();

    if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority()
        || !World || !World->IsGameWorld()
        || bAttackPrepared || !IsValid(InMesh)
        || InMesh->GetWorld() != World
        || TraceSegments.IsEmpty())
    {
        return false;
    }

    // 공격 시작 전 소켓과 반지름 검증
    for (const FTBMeleeTraceSegment& Segment : TraceSegments)
    {
        if (!FMath::IsFinite(Segment.Radius) || Segment.Radius <= 0.0f
            || Segment.StartSocket.IsNone() || Segment.EndSocket.IsNone()
            || !InMesh->DoesSocketExist(Segment.StartSocket)
            || !InMesh->DoesSocketExist(Segment.EndSocket))
        {
            return false;
        }
    }

    TraceMesh = InMesh;
    bAttackPrepared = true;

    AddTickPrerequisiteComponent(InMesh);
    return true;
}

void UTBMeleeTraceComponent::StartTrace()
{
    if (!bAttackPrepared || bTracing || !TraceMesh.IsValid() || !OnHit.IsBound())
    {
        return;
    }

    HitActors.Reset();

    ++TraceRevision;
    bTracing = true;
    SetComponentTickEnabled(true);
    TraceCurrentPose();
}

void UTBMeleeTraceComponent::StopTrace()
{
    ++TraceRevision;
    bTracing = false;

    SetComponentTickEnabled(false);
    HitActors.Reset();
    PreviousPoses.Reset();
}

void UTBMeleeTraceComponent::FinishAttack()
{
    StopTrace();

    if (UPrimitiveComponent* Mesh = TraceMesh.Get())
    {
        RemoveTickPrerequisiteComponent(Mesh);
    }

    TraceMesh.Reset();
    bAttackPrepared = false;
}

void UTBMeleeTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bTracing)
    {
        TraceCurrentPose();
    }
}

void UTBMeleeTraceComponent::TraceCurrentPose()
{
    AActor* OwnerActor = GetOwner();
    UPrimitiveComponent* Mesh = TraceMesh.Get();

    if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(Mesh))
    {
        FinishAttack();
        return;
    }

    // 공격자와 별도 무기 액터 제외
    TArray<AActor*> IgnoredActors;
    IgnoredActors.Add(OwnerActor);

    if (AActor* MeshOwner = Mesh->GetOwner())
    {
        IgnoredActors.AddUnique(MeshOwner);
    }

    const bool bHasPreviousPose = PreviousPoses.Num() == TraceSegments.Num();
    PreviousPoses.SetNum(TraceSegments.Num());
    const uint32 CurrentRevision = TraceRevision;
    TArray<FHitResult> Hits;

    for (int32 Index = 0; Index < TraceSegments.Num(); ++Index)
    {
        const FTBMeleeTraceSegment& Segment = TraceSegments[Index];
        const FSegmentPose Current = {
            Mesh->GetSocketLocation(Segment.StartSocket),
            Mesh->GetSocketLocation(Segment.EndSocket)
        };
        const FSegmentPose Previous = bHasPreviousPose ? PreviousPoses[Index] : Current;
        PreviousPoses[Index] = Current;

        const FVector PreviousBlade = Previous.End - Previous.Start;
        const FVector CurrentBlade = Current.End - Current.Start;
        const double PreviousLength = PreviousBlade.Size();
        const double CurrentLength = CurrentBlade.Size();
        // 시작과 끝 소켓이 같은 구형 판정도 이동 경로를 보간
        const FVector FallbackDirection = CurrentBlade.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
        const FVector PreviousDirection = PreviousBlade.GetSafeNormal(UE_SMALL_NUMBER, FallbackDirection);
        const FVector CurrentDirection = CurrentBlade.GetSafeNormal(UE_SMALL_NUMBER, PreviousDirection);
        const FQuat Swing = FQuat::FindBetweenNormals(PreviousDirection, CurrentDirection);

        // 칼끝의 회전 호와 시작 소켓의 이동을 합산해 빠른 구간에만 중간 검사 추가
        const double Travel = FVector::Distance(Previous.Start, Current.Start)
            + FMath::Abs(CurrentLength - PreviousLength) + FMath::Max(PreviousLength, CurrentLength) * Swing.GetAngle();
        const double Spacing = FMath::Max(0.1f, FMath::Min(MaxTraceStepDistance, Segment.Radius));
        constexpr int32 MaxStepsPerSegment = 32;
        const int32 Steps = FMath::CeilToInt(FMath::Clamp(Travel / Spacing, 1.0, static_cast<double>(MaxStepsPerSegment)));

        for (int32 Step = 1; Step < Steps; ++Step)
        {
            const double Alpha = static_cast<double>(Step) / Steps;
            const FVector Start = FMath::Lerp(Previous.Start, Current.Start, Alpha);
            const FVector Direction = FQuat::Slerp(FQuat::Identity, Swing, Alpha).RotateVector(PreviousDirection);
            const FVector End = Start + Direction * FMath::Lerp(PreviousLength, CurrentLength, Alpha);

            if (!SweepSegment(OwnerActor, Start, End, Segment.Radius, IgnoredActors, Hits, CurrentRevision))
            {
                return;
            }
        }

        if (!SweepSegment(OwnerActor, Current.Start, Current.End, Segment.Radius, IgnoredActors, Hits, CurrentRevision))
        {
            return;
        }
    }
}

bool UTBMeleeTraceComponent::SweepSegment(AActor* OwnerActor, const FVector& Start, const FVector& End, float Radius,
    const TArray<AActor*>& IgnoredActors, TArray<FHitResult>& Hits, uint32 CurrentRevision)
{
    constexpr ECollisionChannel MeleeTraceChannel = ECC_GameTraceChannel3;
    UKismetSystemLibrary::SphereTraceMulti(
        OwnerActor, Start, End, Radius, UEngineTypes::ConvertToTraceType(MeleeTraceChannel),
        false, IgnoredActors, DrawDebugType, Hits, true);

    for (const FHitResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        const TWeakObjectPtr<AActor> TargetKey(Target);
        if (!IsValid(Target) || HitActors.Contains(TargetKey))
        {
            continue;
        }

        HitActors.Add(TargetKey);
        OnHit.Broadcast(Hit);
        if (!bTracing || TraceRevision != CurrentRevision)
        {
            return false;
        }
    }
    return true;
}

void UTBMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FinishAttack();
    Super::EndPlay(EndPlayReason);
}
