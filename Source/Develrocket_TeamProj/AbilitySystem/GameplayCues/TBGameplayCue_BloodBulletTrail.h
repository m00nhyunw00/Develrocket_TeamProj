#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "TBGameplayCue_BloodBulletTrail.generated.h"

class UNiagaraSystem;

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayCue_BloodBulletTrail : public UGameplayCueNotify_Static
{
    GENERATED_BODY()

public:
    // 전달받은 끝점과 발사자의 총구를 연결하여 궤적 재생
    virtual bool OnExecute_Implementation(AActor* MyTarget,const FGameplayCueParameters& Parameters) const override;

protected:
    // 이 Cue에서 재생할 총격 궤적
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TObjectPtr<UNiagaraSystem> TrailSystem;

    // 무기 Static Mesh의 총구 소켓
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    FName MuzzleSocketName = TEXT("weapon_end");
};