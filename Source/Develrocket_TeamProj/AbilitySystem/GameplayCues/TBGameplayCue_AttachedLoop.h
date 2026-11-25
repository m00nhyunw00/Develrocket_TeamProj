#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "TBGameplayCue_AttachedLoop.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API ATBGameplayCue_AttachedLoop : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ATBGameplayCue_AttachedLoop(const FObjectInitializer& ObjectInitializer);

	virtual bool WhileActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	virtual bool Recycle() override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 상태가 유지되는 동안 재생할 Niagara
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> LoopSystem;

	// 대상 메시의 부착 소켓과 소켓 기준 보정값
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FName SocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FVector EffectScale = FVector::OneVector;

private:
	// 현재 큐 인스턴스가 생성한 이펙트
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> ActiveEffect;

	void StopLoopEffect();
};