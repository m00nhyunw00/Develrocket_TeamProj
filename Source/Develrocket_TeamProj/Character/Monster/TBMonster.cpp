#include "TBMonster.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATBMonster::ATBMonster(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// 몬스터가 직접 소유하는 ASC 생성 및 네트워크 복제 활성화
	MonsterASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("MonsterASC"));

	MonsterASC->SetIsReplicated(true);

	// 활성 GameplayEffect 상세 정보는 생략하고 상태 태그·GameplayCue 중심으로 복제
	MonsterASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// 레벨 배치와 런타임 스폰 모두 서버의 AI 자동 빙의 대상으로 설정
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// Controller의 시선 대신 이동 방향을 따라 몸체 회전
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 360.0f, 0.0f);
}

void ATBMonster::BeginPlay()
{
	Super::BeginPlay();

	// 서버와 클라이언트에서 몬스터 자신을 ASC의 OwnerActor·AvatarActor로 연결
	InitializeAbilitySystem(MonsterASC.Get(), this);
}

void ATBMonster::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버의 빙의 이후 ASC 연결 및 Controller 관련 참조 갱신
	InitializeAbilitySystem(MonsterASC.Get(), this);
}
