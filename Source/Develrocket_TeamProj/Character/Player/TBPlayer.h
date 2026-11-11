#pragma once

#include "CoreMinimal.h"
#include "Character/TBCharacterBase.h"
#include "TBPlayer.generated.h"

class UInputAction;
class UCameraComponent;
class UTBInteractionComponent;
struct FInputActionValue;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBPlayer : public ATBCharacterBase
{
	GENERATED_BODY()

public:
	// 생성 및 갱신 ----------------------

	ATBPlayer(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	// 빙의 및 네트워크 초기화 ----------------------

	// 서버 빙의 및 클라이언트의 PlayerState·Controller 갱신 처리
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void OnRep_Controller() override;
	virtual void PawnClientRestart() override;

	// 입력 바인딩 ----------------------

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// 점프·앉기 상태 전환 ----------------------

	// 기본 점프 검사 전에 실제 앉기 상태 해제
	virtual void CheckJumpInput(float DeltaTime) override;

	// 착지 후 유지 중인 앉기 입력 복구
	virtual void Landed(const FHitResult& Hit) override;

	// 앉기 시작·종료에 따른 카메라 높이 갱신
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

	// 공격 및 피해 처리 ----------------------
	
	// 서버 기본 공격 판정, 궤적 연출 요청 및 피해 적용 결과 반환
	bool PerformPrimaryAttackTrace(const FGameplayTag& TrailCueTag);

	// 서버 피해 적용
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Player|Damage")
	bool ApplyDamage(float Damage, AActor* DamageSource);

	// 사망 상태 및 처리 ----------------------

	UFUNCTION(BlueprintPure, Category = "Player|State")
	bool IsDead() const;

	void Die();

	// 사망 태그 변화에 따른 캐릭터 상태 갱신
	void HandleDeadTagChanged(const FGameplayTag Tag, int32 NewCount);

	// 카메라 반동 ----------------------

	// 어빌리티에서 전달한 설정으로 로컬 카메라 반동 시작
	UFUNCTION(BlueprintCallable, Category = "Camera|Recoil")
	void StartCameraRecoil(float InPitch, float InRiseTime, float InReturnTime);

	// 최종 카메라 시점에 반동 적용
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

	// 1인칭 메시 접근 ----------------------

	UFUNCTION(BlueprintPure, Category = "Character|Mesh")
	USkeletalMeshComponent* GetFirstPersonMesh() const
	{
		return FirstPersonMesh.Get();
	}
	
	// 무기 소켓 월드 위치 조회 ----------------------
	
	bool TryGetWeaponSocketLocation(FName SocketName, FVector& OutLocation) const;

protected:
	virtual void BeginPlay() override;

	// 카메라 및 높이 설정 ----------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// 앉기·일어서기 시 카메라 높이 전환 속도
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.1"))
	float CameraHeightInterpSpeed = 20.0f;

	// 카메라 반동 설정 ----------------------

	// 캐릭터별 반동 강도 배율
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Recoil", meta = (ClampMin = "0.0"))
	float RecoilMultiplier = 1.0f;

	// 연속 발사 시 최대 누적 반동 각도
	UPROPERTY(EditDefaultsOnly, Category = "Camera|Recoil", meta = (ClampMin = "0.0"))
	float MaxRecoilPitch = 1.0f;

	// 1인칭 팔 메시 ----------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	// 무기 메시 및 부착 설정 ----------------------

	// 소유 플레이어에게 표시할 총
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> FirstPersonWeaponMesh;

	// 다른 플레이어에게 표시할 총
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UStaticMeshComponent> ThirdPersonWeaponMesh;

	// 두 무기 컴포넌트에서 공유할 총 메시
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TObjectPtr<UStaticMesh> WeaponMeshAsset;

	// 팔 메시와 전신 메시에서 사용할 공통 소켓 이름
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FName WeaponSocketName = TEXT("GunSocket");

	// 상호작용 탐지 ----------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UTBInteractionComponent> InteractionComponent;

	// 이동·시점 입력 액션 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// 점프·앉기 입력 액션 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CrouchAction;

	// 공격·스킬·재장전 입력 액션 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> PrimaryAttackAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SkillAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	// 상호작용·일시정지 입력 액션 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> PauseAction;

private:
	// GAS 초기화 ----------------------

	// PlayerState의 ASC를 현재 캐릭터에 연결
	void InitializeFromPlayerState();

	// 메시 및 카메라 초기 설정 ----------------------

	// 메시의 표시·애니메이션 모드·충돌 규칙 적용
	void ConfigurePlayerMeshes();

	// 무기 메시 적용 및 소켓 부착
	void InitializeWeaponMeshes();

	// 현재 눈높이에 맞춘 카메라 위치 설정
	void UpdateCameraHeight();

	// 로컬 HUD 연결 ----------------------

	// 소유 플레이어 HUD와 PlayerState의 ASC 연결
	void TryBindLocalHUD();

	// 피격 및 사망 처리 상태 ----------------------

	// 공격 인터페이스를 통한 피격 처리
	virtual bool ReceiveAttackHit_Implementation(const FGameplayTag& DamageType,float Damage,AActor* DamageSource,const FHitResult& HitResult) override;

	// 사망 처리 중복 방지
	bool bDeathHandled = false;

	// 카메라 반동 계산 및 실행 상태 ----------------------

	// 현재 시간의 반동 각도 계산
	float GetCameraRecoilPitch() const;

	// 현재 반동의 시작 시각과 각도
	double RecoilStartTime = -1.0;
	float RecoilStartPitch = 0.0f;
	float RecoilPeakPitch = 0.0f;

	// 이번 발사에서 전달받은 상승·복귀 시간
	float ActiveRecoilRiseTime = 0.05f;
	float ActiveRecoilReturnTime = 0.18f;

	// 이동·시점 입력 처리 ----------------------

	void InputMove(const FInputActionValue& Value);
	void InputLook(const FInputActionValue& Value);

	// 점프 입력 처리 ----------------------

	void InputJumpStarted();
	void InputJumpStopped();

	// 앉기 입력 및 유지 상태 ----------------------

	void InputCrouchStarted();
	void InputCrouchStopped();

	// 실제 앉기 상태와 별도로 보관하는 Ctrl 입력 의도
	bool bCrouchInputHeld = false;

	// 기본 공격 입력 처리 ----------------------

	void InputPrimaryAttackStarted();
	void InputPrimaryAttackStopped();

	// 스킬 입력 처리 ----------------------

	void InputSkillStarted();
	void InputSkillStopped();

	// 재장전 입력 처리 ----------------------

	void InputReloadStarted();

	// 상호작용·일시정지 입력 처리 ----------------------

	void InputInteractStarted();
	void InputPauseStarted();
};
