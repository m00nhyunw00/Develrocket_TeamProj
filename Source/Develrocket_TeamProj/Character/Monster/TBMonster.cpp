#include "TBMonster.h"

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ATBMonster::ATBMonster(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// 몬스터가 직접 소유하는 ASC 생성
	MonsterASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("MonsterASC"));
	MonsterASC->SetIsReplicated(true);
	MonsterASC->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// 기본 AIController 사용 및 서버의 자동 빙의 설정
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// 이동 방향을 따라 몸체 회전
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 360.0f, 0.0f);

	// 몬스터 전신 메시의 기본 표시 및 애니메이션 모드 설정
	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->SetOnlyOwnerSee(false);
	GetMesh()->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	GetMesh()->SetRelativeLocation(FVector(0.0f, 0.0f, -GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()));
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
