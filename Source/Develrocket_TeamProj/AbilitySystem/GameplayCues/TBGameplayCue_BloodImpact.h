#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "TBGameplayCue_BloodImpact.generated.h"

class UNiagaraSystem;
class UMaterialInterface;

// 혈탄 충돌 시 혈탄·대상 혈액 파티클과 데칼을 처리하는 공통 Cue
UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayCue_BloodImpact : public UGameplayCueNotify_Static
{
    GENERATED_BODY()

public:
    // 상위 Gameplay Cue 태그의 중복 실행 방지 설정
    UTBGameplayCue_BloodImpact();

    // 순간 피격 연출에 해당하는 Executed 이벤트만 허용
    virtual bool HandlesEvent(EGameplayCueEvent::Type EventType) const override;

    // 전달받은 충돌 정보로 파티클과 데칼 생성
    virtual bool OnExecute_Implementation(
        AActor* MyTarget,
        const FGameplayCueParameters& Parameters) const override;

protected:
    // 모든 충돌에서 붉은색으로 재생할 혈탄 파티클
    UPROPERTY(EditDefaultsOnly, Category = "Impact|Bullet")
    TObjectPtr<UNiagaraSystem> BulletImpactSystem = nullptr;

    // 생명체의 혈액 파티클, 비생명체용 Cue에서는 미지정
    UPROPERTY(EditDefaultsOnly, Category = "Impact|Target")
    TObjectPtr<UNiagaraSystem> TargetBloodSystem = nullptr;

    // 대상 혈액 파티클과 데칼에 적용할 색상
    UPROPERTY(EditDefaultsOnly, Category = "Impact|Target")
    FLinearColor TargetBloodColor = FLinearColor(0.4f, 0.01f, 0.01f, 1.0f);

    // 충돌 대상에 표시할 환경 혈흔 또는 생명체 상처 머티리얼
    UPROPERTY(EditDefaultsOnly, Category = "Impact|Decal")
    TObjectPtr<UMaterialInterface> DecalMaterial = nullptr;

    // 데칼 투영 크기, X는 투영 깊이이며 Y·Z는 표면 크기
    UPROPERTY(EditDefaultsOnly, Category = "Impact|Decal")
    FVector DecalSize = FVector(4.0f, 12.0f, 12.0f);

    // 생명체의 충돌한 본에 데칼을 부착할지 여부
    UPROPERTY(EditDefaultsOnly, Category = "Impact|Decal")
    bool bAttachDecalToHitBone = false;

    // 충돌 표면의 바깥쪽으로 연출 위치를 띄울 거리
    UPROPERTY(EditDefaultsOnly, Category = "Impact",
        meta = (ClampMin = "0.0"))
    float SurfaceOffset = 0.5f;

    // 데칼 생성 후 페이드 시작 전까지 유지할 시간
    UPROPERTY(EditDefaultsOnly, Category = "Impact|Decal",
        meta = (ClampMin = "0.0"))
    float DecalHoldTime = 8.0f;

    // 데칼의 페이드가 진행되는 시간
    UPROPERTY(EditDefaultsOnly, Category = "Impact|Decal",
        meta = (ClampMin = "0.01"))
    float DecalFadeDuration = 1.0f;
};