#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "TBGameplayCue_AttackVisual.generated.h"

class UNiagaraSystem;

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayCue_AttackVisual : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> NormalSystem;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> EnhancedSystem;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FName SocketName = NAME_None;

	// SocketName과 함께 도신 전체 트레일의 양쪽 경계로 사용할 소켓
	UPROPERTY(EditDefaultsOnly, Category = "Effects|Trail")
	FName TrailEndSocketName = NAME_None;

	// 도신 구간을 샘플링할 Niagara Skeletal Mesh 파라미터, 소켓 인덱스 0=시작, 1=끝
	UPROPERTY(EditDefaultsOnly, Category = "Effects|Trail")
	FName TrailMeshParameterName = TEXT("User.TrailMesh");

	// 기존 Notify에서 사용하던 부착 위치 보정
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FVector EffectScale = FVector::OneVector;

	// 루프형 시스템도 지정 시간 이후 정리
	UPROPERTY(EditDefaultsOnly, Category = "Effects", meta = (ClampMin = "0.01"))
	float EffectDuration = 0.5f;
};
