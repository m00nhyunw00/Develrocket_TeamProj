#include "Components/TBMeleeTraceComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UTBMeleeTraceComponent::UTBMeleeTraceComponent()
{
    // 판정 구간에만 Tick 활성화
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;

    // 기본 피해 후보는 Pawn ObjectType
    TargetObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
}

void UTBMeleeTraceComponent::SetTraceMesh(FName MeshKey, UPrimitiveComponent* Mesh)
{
    if (MeshKey.IsNone())
    {
        return;
    }

    // 실행 도중 검사 메시가 바뀌면 기존 구간 종료
    StopAllTraces();

    if (IsValid(Mesh))
    {
        TraceMeshes.Add(MeshKey, Mesh);
    }
    else
    {
        TraceMeshes.Remove(MeshKey);
    }
}

bool UTBMeleeTraceComponent::BeginWindow(FName ProfileName, USkeletalMeshComponent* SourceMesh, const UObject* WindowOwner, int32 MontageInstanceID)
{
    AActor* OwnerActor = GetOwner();
    UWorld* World = GetWorld();

    // 서버의 게임 월드에서 하나의 구간만 실행
    if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority()
        || !World || !World->IsGameWorld()
        || bTracing || !IsValid(SourceMesh)
        || !IsValid(WindowOwner) || MontageInstanceID == INDEX_NONE)
    {
        return false;
    }

    const FTBMeleeTraceProfile* Profile = TraceProfiles.Find(ProfileName);
    if (!Profile || Profile->Segments.IsEmpty())
    {
        return false;
    }

    // 설정한 ObjectType으로 검사 조건 구성
    FCollisionObjectQueryParams ObjectParams;

    for (const TEnumAsByte<EObjectTypeQuery> ObjectType : TargetObjectTypes)
    {
        const ECollisionChannel Channel = UEngineTypes::ConvertToCollisionChannel(ObjectType.GetValue());

        if (Channel >= ECC_MAX || !FCollisionObjectQueryParams::IsValidObjectQuery(Channel))
        {
            return false;
        }

        ObjectParams.AddObjectTypesToQuery(Channel);
    }

    if (!ObjectParams.IsValid())
    {
        return false;
    }

    // 모든 소켓을 검증한 뒤에만 실행 상태로 전환
    TArray<FRunningTrace> NewTraces;

    for (const FTBMeleeTraceSegment& Segment : Profile->Segments)
    {
        UPrimitiveComponent* Mesh = TraceMeshes.FindRef(Segment.MeshKey).Get();

        if (!IsValid(Mesh) || Mesh->GetWorld() != World
            || !FMath::IsFinite(Segment.Radius) || Segment.Radius <= 0.0f
            || Segment.StartSocket.IsNone() || Segment.EndSocket.IsNone()
            || !Mesh->DoesSocketExist(Segment.StartSocket)
            || !Mesh->DoesSocketExist(Segment.EndSocket))
        {
            return false;
        }

        FRunningTrace& Trace = NewTraces.AddDefaulted_GetRef();
        Trace.Settings = Segment;
        Trace.Mesh = Mesh;
        Trace.PreviousStart = Mesh->GetSocketLocation(Segment.StartSocket);
        Trace.PreviousEnd = Mesh->GetSocketLocation(Segment.EndSocket);

        if (Trace.PreviousStart.ContainsNaN() || Trace.PreviousEnd.ContainsNaN())
        {
            return false;
        }
    }

    // 공격자와 등록된 무기 액터를 검사에서 제외
    ActiveQueryParams = FCollisionQueryParams(SCENE_QUERY_STAT(TBMeleeTrace), false);
    ActiveQueryParams.bFindInitialOverlaps = true;
    ActiveQueryParams.AddIgnoredActor(OwnerActor);

    for (const auto& Entry : TraceMeshes)
    {
        if (UPrimitiveComponent* Mesh = Entry.Value.Get())
        {
            if (AActor* MeshOwner = Mesh->GetOwner())
            {
                ActiveQueryParams.AddIgnoredActor(MeshOwner);
            }
        }
    }

    ActiveObjectParams = ObjectParams;
    RunningTraces = MoveTemp(NewTraces);
    HitActors.Reset();

    ActiveSourceMesh = SourceMesh;
    ActiveWindowOwner = WindowOwner;
    ActiveMontageInstanceID = MontageInstanceID;

    // 애니메이션 및 검사 메시 갱신 이후 판정
    AddTickPrerequisiteComponent(SourceMesh);

    for (const FRunningTrace& Trace : RunningTraces)
    {
        if (UPrimitiveComponent* Mesh = Trace.Mesh.Get())
        {
            AddTickPrerequisiteComponent(Mesh);
        }
    }

    ++Generation;
    bTracing = true;
    SetComponentTickEnabled(true);

    return true;
}

void UTBMeleeTraceComponent::EndWindow(USkeletalMeshComponent* SourceMesh, const UObject* WindowOwner, int32 MontageInstanceID)
{
    // 다른 ANS 또는 이전 몽타주의 종료 요청 제외
    if (bTracing
        && ActiveSourceMesh.Get() == SourceMesh
        && ActiveWindowOwner.Get() == WindowOwner
        && ActiveMontageInstanceID == MontageInstanceID)
    {
        StopAllTraces();
    }
}

void UTBMeleeTraceComponent::StopAllTraces()
{
    // 타격 콜백에서 진행 중인 실행도 중단되도록 세대 변경
    ++Generation;
    bTracing = false;
    SetComponentTickEnabled(false);

    if (USkeletalMeshComponent* Mesh = ActiveSourceMesh.Get())
    {
        RemoveTickPrerequisiteComponent(Mesh);
    }

    for (const FRunningTrace& Trace : RunningTraces)
    {
        if (UPrimitiveComponent* Mesh = Trace.Mesh.Get())
        {
            RemoveTickPrerequisiteComponent(Mesh);
        }
    }

    RunningTraces.Reset();
    HitActors.Reset();
    ActiveSourceMesh.Reset();
    ActiveWindowOwner.Reset();
    ActiveMontageInstanceID = INDEX_NONE;
}

void UTBMeleeTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bTracing)
    {
        return;
    }

    AActor* OwnerActor = GetOwner();
    USkeletalMeshComponent* SourceMesh = ActiveSourceMesh.Get();
    UAnimInstance* AnimInstance = IsValid(SourceMesh) ? SourceMesh->GetAnimInstance() : nullptr;

    FAnimMontageInstance* MontageInstance = AnimInstance
        ? AnimInstance->GetMontageInstanceForID(ActiveMontageInstanceID)
        : nullptr;

    // 소유자·몽타주가 사라지거나 블렌드아웃되면 판정 종료
    if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority()
        || !ActiveWindowOwner.IsValid()
        || !MontageInstance || !MontageInstance->IsActive())
    {
        StopAllTraces();
        return;
    }

    const uint32 CurrentGeneration = Generation;

    for (int32 Index = 0; Index < RunningTraces.Num(); ++Index)
    {
        FRunningTrace& Trace = RunningTraces[Index];
        UPrimitiveComponent* Mesh = Trace.Mesh.Get();

        if (!IsValid(Mesh)
            || !Mesh->DoesSocketExist(Trace.Settings.StartSocket)
            || !Mesh->DoesSocketExist(Trace.Settings.EndSocket))
        {
            StopAllTraces();
            return;
        }

        const FVector Start = Mesh->GetSocketLocation(Trace.Settings.StartSocket);
        const FVector End = Mesh->GetSocketLocation(Trace.Settings.EndSocket);

        if (Start.ContainsNaN() || End.ContainsNaN())
        {
            StopAllTraces();
            return;
        }

        // 콜백에서 실행 정보가 변경될 수 있으므로 필요한 값 보관
        const FVector PreviousStart = Trace.PreviousStart;
        const FVector PreviousEnd = Trace.PreviousEnd;
        const float Radius = Trace.Settings.Radius;

        Trace.PreviousStart = Start;
        Trace.PreviousEnd = End;

        // 소켓 이동량에 따라 프레임 사이를 제한된 횟수로 보간
        const double Movement = FMath::Max(
            FVector::Distance(PreviousStart, Start),
            FVector::Distance(PreviousEnd, End));

        const int32 StepLimit = FMath::Clamp(MaxSubsteps, 1, 8);
        const double DesiredSteps = FMath::CeilToDouble(Movement / FMath::Max(Radius, 1.0f));
        const int32 Steps = static_cast<int32>(FMath::Clamp(DesiredSteps, 1.0, static_cast<double>(StepLimit)));

        for (int32 Step = 1; Step <= Steps; ++Step)
        {
            const float Alpha = static_cast<float>(Step) / Steps;
            const FVector SampleStart = FMath::Lerp(PreviousStart, Start, Alpha);
            const FVector SampleEnd = FMath::Lerp(PreviousEnd, End, Alpha);

            if (!SweepSegment(SampleStart, SampleEnd, Radius, CurrentGeneration))
            {
                return;
            }
        }
    }
}

bool UTBMeleeTraceComponent::SweepSegment(const FVector& Start, const FVector& End, float Radius, uint32 ExpectedGeneration)
{
    UWorld* World = GetWorld();
    AActor* OwnerActor = GetOwner();

    if (!World || !IsValid(OwnerActor))
    {
        StopAllTraces();
        return false;
    }

    // 지정한 ObjectType에 해당하는 여러 충돌 결과 수집
    TArray<FHitResult> Hits;
    World->SweepMultiByObjectType(
        Hits, Start, End, FQuat::Identity,
        ActiveObjectParams, FCollisionShape::MakeSphere(Radius), ActiveQueryParams);

    for (const FHitResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        const TWeakObjectPtr<AActor> TargetKey(Target);

        if (!IsValid(Target) || Target == OwnerActor || HitActors.Contains(TargetKey))
        {
            continue;
        }

        // Pawn 조회만으로는 벽이 자동 차단되지 않으므로 별도 확인
        if (bCheckOcclusion)
        {
            FVector EyeLocation;
            FRotator EyeRotation;
            OwnerActor->GetActorEyesViewPoint(EyeLocation, EyeRotation);

            const FVector TargetPoint = Hit.bStartPenetrating ? Target->GetActorLocation() : FVector(Hit.ImpactPoint);

            FHitResult SightHit;
            const bool bBlocked = World->LineTraceSingleByChannel(
                SightHit, EyeLocation, TargetPoint, OcclusionChannel.GetValue(), ActiveQueryParams);

            if (bBlocked && SightHit.GetActor() != Target)
            {
                continue;
            }
        }

        // 같은 대상의 다른 컴포넌트·소켓 구간에서 발생한 중복 알림 방지
        HitActors.Add(TargetKey);
        OnHit.Broadcast(Hit);

        // 구독자가 어빌리티를 종료하거나 판정을 다시 시작한 경우 중단
        if (!bTracing || Generation != ExpectedGeneration)
        {
            return false;
        }
    }

    return true;
}

void UTBMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 소유 캐릭터 종료 시 판정과 실행 정보 정리
    StopAllTraces();
    Super::EndPlay(EndPlayReason);
}