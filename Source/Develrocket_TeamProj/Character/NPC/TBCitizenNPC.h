#pragma once

#include "CoreMinimal.h"
#include "Character/TBCharacterBase.h"
#include "Interface/TBInteractable.h"
#include "TBDefine.h"
#include "TBCitizenNPC.generated.h"

class UAnimMontage;

UCLASS()
class DEVELROCKET_TEAMPROJ_API ATBCitizenNPC : public ATBCharacterBase, public ITBInteractable
{
	GENERATED_BODY()
	
public:
	// 부모(ATBCharacterBase)가 FObjectInitializer 생성자를 쓰므로 똑같이 받아서 넘겨야함
	ATBCitizenNPC(const FObjectInitializer& ObjectInitializer);
	
	// 어떤 함수를 네트워크로 복제할지 등록하는 함수
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	// 상호작용 계약 구현 ----------------------
	virtual bool CanInteract_Implementation(AActor* InteractInstigator) const override;   // 흡혈 가능한 상태인지
	virtual void Interact_Implementation(AActor* InteractInstigator) override;            // [서버] 몽타주 끝 → 혈액 회복 + 사망
	virtual void InteractLocal_Implementation(AActor* InteractInstigator) override;       // [흡혈한 플레이어 클라] 개인 연출
	virtual FText GetInteractPrompt_Implementation() const override;                      // HUD 문구
	virtual UAnimMontage* GetInteractMontage_Implementation() const override;             // 플레이어 3인칭 흡혈 몽타주
	virtual UAnimMontage* GetInteractFirstPersonMontage_Implementation() const override;  // 플레이어 1인칭 흡혈 몽타주
	virtual bool ShouldResolveOnStart_Implementation() const override;                    // false = 몽타주가 끝나야 결과
	
protected:
	virtual void BeginPlay() override;                        // 서버·클라이언트 모두에서 ASC 연결
   	virtual void PossessedBy(AController* NewController) override;   // 서버에서 AI가 빙의한 뒤 ASC 정보 갱신

	// 상호작용 데이터 ----------------------
	UPROPERTY(EditAnywhere, Category = "Interaction")
	FText InteractPrompt;
	
	UPROPERTY(EditAnywhere, Category = "Interaction|Animation")
	TObjectPtr<UAnimMontage> FeedMontage;
	
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Animation")
	TObjectPtr<UAnimMontage> FeedFirstPersonMontage;
	
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Animation")
	TObjectPtr<UAnimMontage> VictimMontage;
	
	UPROPERTY(EditDefaultsOnly, Category = "Interaction|Animation", meta=(Units = "cm"))
	float MaxDrainDistance = 250.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Citizen", meta=(Units="s"))
	float CorpseLifeSpan = 10.f;
	
	// 상태 --------------------
	// 값이 바뀌면 클라이언트에서 OnRep_CitizenState 자동호출
	UPROPERTY(ReplicatedUsing = OnRep_CitizenState)
	ETBCitizenState CitizenState = ETBCitizenState::Idle;
	
	UFUNCTION()
	void OnRep_CitizenState();
	void SetCitizenState(ETBCitizenState NewState);
	
};