#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayTagContainer.h"
#include "TBGameplayCue_AttackVisual.generated.h"

class UNiagaraSystem;
class USkeletalMeshComponent;
class UWorld;

// 공격 Cue 태그별 공통 연출 설정
USTRUCT(BlueprintType)
struct DEVELROCKET_TEAMPROJ_API FTBAttackVisualSettings
{
	GENERATED_BODY()

	// 실제 호출된 하위 Cue 태그와 정확히 일치하는 설정 선택
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects", meta = (Categories = "GameplayCue"))
	FGameplayTag CueTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	TObjectPtr<UNiagaraSystem> NormalSystem = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	FName SocketName = NAME_None;

	// SocketName과 함께 도신 전체 트레일의 양쪽 경계로 사용할 소켓
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Trail")
	FName TrailEndSocketName = NAME_None;

	// 도신 구간을 샘플링할 Niagara Skeletal Mesh 파라미터, 소켓 인덱스 0=시작, 1=끝
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Trail")
	FName TrailMeshParameterName = TEXT("User.TrailMesh");

	// 기존 Notify에서 사용하던 부착 위치 보정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects")
	FVector EffectScale = FVector::OneVector;

	// 루프형 시스템도 지정 시간 이후 정리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects", meta = (ClampMin = "0.01"))
	float EffectDuration = 0.5f;

	// 총구와 서버 판정 끝점을 연결할 총격 궤적
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Beam")
	TObjectPtr<UNiagaraSystem> TrailSystem = nullptr;

	// 일반 공격의 총구 발사 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Muzzle")
	TObjectPtr<UNiagaraSystem> MuzzleImpactSystem = nullptr;

	// 총구 발사 이펙트의 상대 크기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Muzzle")
	FVector MuzzleImpactScale = FVector(0.8f);

	// 다른 플레이어 화면의 총구 이펙트 방출 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Muzzle", meta = (ClampMin = "0.001"))
	float MuzzleImpactDuration = 0.10428184f;

	// 본인 1인칭 화면의 총구 이펙트 방출 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Muzzle", meta = (ClampMin = "0.001"))
	float FirstPersonMuzzleImpactDuration = 0.0333333f;

	// 총격 궤적과 발사 이펙트가 공유할 캐릭터 메시의 총구 소켓
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effects|Muzzle")
	FName MuzzleSocketName = TEXT("weapon_end");
};

UCLASS(Blueprintable)
class DEVELROCKET_TEAMPROJ_API UTBGameplayCue_AttackVisual : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

protected:
	// 하나의 Cue BP에서 관리할 공격 태그별 연출 목록
	UPROPERTY(EditDefaultsOnly, Category = "AttackVisuals", meta = (TitleProperty = "CueTag"))
	TArray<FTBAttackVisualSettings> AttackVisuals;

	// 실제 호출 태그에 사용할 실행별 연출 설정 복사
	virtual bool ResolveSettings(const FGameplayCueParameters& Parameters, FTBAttackVisualSettings& OutSettings) const;

private:
	// 단일 소켓 부착 이펙트 또는 두 소켓 도신 트레일 재생
	bool SpawnAttachedVisual(UWorld* World, USkeletalMeshComponent* Mesh, const FTBAttackVisualSettings& Settings) const;

	// 시점별 총구 위치와 고정된 끝점으로 총격 궤적 재생
	bool SpawnBeam(UWorld* World, USkeletalMeshComponent* Mesh, const FVector& TrailEnd, bool bFirstPerson, const FTBAttackVisualSettings& Settings) const;

	// 총구 발사 이펙트 재생 및 시점별 방출 시간 적용
	bool SpawnMuzzle(UWorld* World, USkeletalMeshComponent* Mesh, bool bFirstPerson, const FTBAttackVisualSettings& Settings) const;
};
