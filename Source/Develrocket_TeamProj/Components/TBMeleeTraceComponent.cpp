#include "Components/TBMeleeTraceComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UTBMeleeTraceComponent::UTBMeleeTraceComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;

    TraceObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
}

bool UTBMeleeTraceComponent::PrepareAttack(UPrimitiveComponent* InMesh)
{
    AActor* OwnerActor = GetOwner();
    UWorld* World = GetWorld();

    if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority()
        || !World || !World->IsGameWorld()
        || bAttackPrepared || !IsValid(InMesh)
        || InMesh->GetWorld() != World
        || TraceSegments.IsEmpty() || TraceObjectTypes.IsEmpty())
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
}

void UTBMeleeTraceComponent::StopTrace()
{
    ++TraceRevision;
    bTracing = false;

    SetComponentTickEnabled(false);
    HitActors.Reset();
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

    if (!bTracing)
    {
        return;
    }

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

    const uint32 CurrentRevision = TraceRevision;

    for (const FTBMeleeTraceSegment& Segment : TraceSegments)
    {
        const FVector Start = Mesh->GetSocketLocation(Segment.StartSocket);
        const FVector End = Mesh->GetSocketLocation(Segment.EndSocket);

        TArray<FHitResult> Hits;

        UKismetSystemLibrary::SphereTraceMultiForObjects(
            OwnerActor, Start, End, Segment.Radius,
            TraceObjectTypes, false, IgnoredActors,
            DrawDebugType, Hits, true);

        for (const FHitResult& Hit : Hits)
        {
            AActor* Target = Hit.GetActor();
            const TWeakObjectPtr<AActor> TargetKey(Target);

            if (!IsValid(Target) || HitActors.Contains(TargetKey))
            {
                continue;
            }

            // 다른 소켓이나 프레임에서 검출되어도 한 번만 전달
            HitActors.Add(TargetKey);
            OnHit.Broadcast(Hit);

            // 피해 처리 도중 종료되거나 재시작된 판정의 후속 처리 중단
            if (!bTracing || TraceRevision != CurrentRevision)
            {
                return;
            }
        }
    }
}

void UTBMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FinishAttack();
    Super::EndPlay(EndPlayReason);
}