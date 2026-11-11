#include "Character/NPC/TBCitizenNPC.h"

#include "Net/UnrealNetwork.h"                    // DOREPLIFETIME 매크로
#include "Components/CapsuleComponent.h"          // 사망 시 캡슐 충돌 변경
#include "GameFramework/CharacterMovementComponent.h"
#include "AI/NPC/TBCitizenAIController.h"             // 도망 AI
#include "Framework/TBPlayerState.h"              // ApplyBloodRecovery
#include "AbilitySystem/TBAbilitySystemComponent.h"   // 시민 전용 ASC
#include "TBGameplayTags.h"                           // 사망 태그

// 생성자 =====================
ATBCitizenNPC::ATBCitizenNPC(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)                    
{
	// 이 캐릭터를 조종할 AI 지정 + 배치/스폰 시 서버에서 자동 빙의
	AIControllerClass = ATBCitizenAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->MaxWalkSpeed = 450.f;               // 부모 기본값 750보다 느리게 → 플레이어가 따라잡을 수 있게
	MoveComp->bOrientRotationToMovement = true;   // 이동하는 방향으로 몸을 돌림
	bUseControllerRotationYaw = false;            // 컨트롤러 회전 대신 이동 방향 회전 사용

	InteractPrompt = FText::FromString(TEXT("흡혈하기"));
	
	// 시민이 직접 소유하는 ASC 생성 (부모의 이동 차단 판정이 "ASC 연결됨"을 요구하기 때문)
	AbilitySystemComponent = CreateDefaultSubobject<UTBAbilitySystemComponent>(TEXT("CitizenASC"));
	AbilitySystemComponent->SetIsReplicated(true);                                      // 태그를 클라이언트에도 복제
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal); // AI용: 최소한만 복제 (몬스터와 동일)
}

// 복제 등록 =====================
void ATBCitizenNPC::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);     // 부모가 등록한 복제 변수 유지 (빠뜨리면 이동 복제 등이 깨짐)
	DOREPLIFETIME(ATBCitizenNPC, CitizenState);              // CitizenState를 모든 클라이언트에 복제
}

// 상호작용 =====================
bool ATBCitizenNPC::CanInteract_Implementation(AActor* InteractInstigator) const
{
	return CitizenState == ETBCitizenState::Idle;  // 평상시에만 흡혈 가능 (흡혈 중/사망 시 프롬프트도 안 뜸)
}

void ATBCitizenNPC::Interact_Implementation(AActor* InteractInstigator)
{
	// GA가 "흡혈 몽타주가 끝난 시점"에 서버에서 호출

	if (!HasAuthority() || CitizenState == ETBCitizenState::Dead)   // 서버만, 이미 죽었으면 중복 처리 방지
	{
		return;
	}

	// 몽타주 도중 시민이 도망쳐 멀어졌다면 흡혈 실패
	if (!InteractInstigator || FVector::Dist(GetActorLocation(), InteractInstigator->GetActorLocation()) > MaxDrainDistance)
	{
		return;
	}

	// 상호작용한 쪽은 플레이어 캐릭터(Pawn) → 그 PlayerState에 ASC와 회복 GE가 있음
	const APawn* InstigatorPawn = Cast<APawn>(InteractInstigator);
	if (ATBPlayerState* TBPlayerState = InstigatorPawn ? InstigatorPawn->GetPlayerState<ATBPlayerState>() : nullptr)
	{
		TBPlayerState->ApplyBloodRecovery();         // BloodRecoveryEffect(GE) 적용 → Blood 회복
	}

	SetCitizenState(ETBCitizenState::Dead);        // 흡혈 완료 → 사망
}

void ATBCitizenNPC::InteractLocal_Implementation(AActor* InteractInstigator)
{
	// [흡혈한 플레이어의 클라이언트에서만] 화면 효과, 사운드 등 개인 연출 자리
}

FText ATBCitizenNPC::GetInteractPrompt_Implementation() const
{
	return InteractPrompt;
}

UAnimMontage* ATBCitizenNPC::GetInteractMontage_Implementation() const
{
	return FeedMontage;
}

UAnimMontage* ATBCitizenNPC::GetInteractFirstPersonMontage_Implementation() const
{
	return FeedFirstPersonMontage;
}

bool ATBCitizenNPC::ShouldResolveOnStart_Implementation() const
{
	return false;                                  // 흡혈은 몽타주가 끝나야 결과 (대화 NPC는 true)
}

// 상태 =====================
void ATBCitizenNPC::SetCitizenState(ETBCitizenState NewState)
{
	if (!HasAuthority() || CitizenState == NewState)   // 서버만 바꿀 수 있고, 같은 값이면 무시
	{
		return;
	}

	CitizenState = NewState;                       // 이 값이 클라이언트로 복제됨

	// 서버 전용 처리
	if (NewState == ETBCitizenState::Dead)
	{
		AbilitySystemComponent->AddLooseGameplayTag(TBGameplayTags::State_Dead);
		if (ATBCitizenAIController* CitizenAI = Cast<ATBCitizenAIController>(GetController()))
		{
			CitizenAI->StopFleeing();              // 도망 AI 정지
		}
		SetLifeSpan(CorpseLifeSpan);               // 일정 시간 뒤 자동 Destroy
	}

	OnRep_CitizenState();                          // 서버에서는 OnRep이 자동 호출되지 않으므로 직접 호출
}

void ATBCitizenNPC::OnRep_CitizenState()
{
	// 서버와 모든 클라이언트에서 실행: "보이는 것"만 처리
	switch (CitizenState)
	{
	case ETBCitizenState::Draining:
		PlayAnimMontage(VictimMontage);            // 흡혈당하는 몽타주 (선택 사항의 시작 훅을 넣어야 실제로 들어옴)
		break;

	case ETBCitizenState::Dead:
		StopAnimMontage();                         // 재생 중인 몽타주 정지
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);   // 상호작용 탐지(Visibility)에 더 이상 안 걸리게
		// 사망 몽타주 / 래그돌 연출 자리
		break;

	default:
		break;
	}
}

void ATBCitizenNPC::BeginPlay()
{
	Super::BeginPlay();

	// 서버와 클라이언트 모두에서 시민 자신을 ASC의 Owner·Avatar로 연결 → IsMovementBlocked()가 false가 됨
	InitializeAbilitySystem(AbilitySystemComponent.Get(), this);
}

void ATBCitizenNPC::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서 AI 컨트롤러가 빙의한 뒤 ASC가 컨트롤러 정보를 갱신하도록 다시 호출 (이미 연결돼 있으면 Refresh만 함)
	InitializeAbilitySystem(AbilitySystemComponent.Get(), this);
}