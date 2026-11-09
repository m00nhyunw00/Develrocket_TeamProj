#pragma once

#include "CoreMinimal.h"
#include "Character/TBCharacterBase.h"
#include "TBPlayer.generated.h"

class UInputAction;
struct FInputActionValue;
class UCameraComponent;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBPlayer : public ATBCharacterBase
{
	GENERATED_BODY()
	
protected:
	// 앉기·일어서기 시 카메라 높이 전환 속도
	UPROPERTY(EditDefaultsOnly, Category = "Camera", meta = (ClampMin = "0.1"))
	float CameraHeightInterpSpeed = 20.0f;

public:
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

	// 1인칭 메시 접근 ----------------------

	UFUNCTION(BlueprintPure, Category = "Character|Mesh")
	USkeletalMeshComponent* GetFirstPersonMesh() const
	{
		return FirstPersonMesh.Get();
	}

protected:
	virtual void BeginPlay() override;

	// 1인칭 카메라 ----------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	// 1인칭 팔 메시 ----------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;
	
	// 무기 메시 ----------------------

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

	// 이동·시점 입력 액션 ----------------------

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

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
	
	// 상호작용 입력 액션 ----------------------
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

private:
	// 입력 유지 상태 ----------------------

	// 실제 앉기 상태와 별도로 보관하는 Ctrl 입력 의도
	bool bCrouchInputHeld = false;

	// GAS 초기화 ----------------------

	// PlayerState의 ASC를 현재 캐릭터에 연결
	void InitializeFromPlayerState();
	
	// 메시 및 카메라 설정 ----------------------

	// 메시의 표시·애니메이션 모드·충돌 규칙 적용
	void ConfigurePlayerMeshes();

	void UpdateCameraHeight();

	// 이동·시점 입력 처리 ----------------------

	void InputMove(const FInputActionValue& Value);
	void InputLook(const FInputActionValue& Value);

	// 점프 입력 처리 ----------------------

	void InputJumpStarted();
	void InputJumpStopped();

	// 앉기 입력 처리 ----------------------

	void InputCrouchStarted();
	void InputCrouchStopped();

	// 기본 공격 입력 처리 ----------------------

	void InputPrimaryAttackStarted();
	void InputPrimaryAttackStopped();

	// 스킬 입력 처리 ----------------------

	void InputSkillStarted();
	void InputSkillStopped();
	
	// 재장전 입력 처리 ----------------------
	void InputReloadStarted();
	
	// 상호작용 입력 처리 ----------------------
	void InputInteractStarted();
	
private:
	// 무기 메시 적용 및 소켓 부착 ----------------------
	void InitializeWeaponMeshes();
	
	// 
	void TryBindLocalHUD();
};
