#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TBCombatInterface.generated.h"

class USkeletalMeshComponent;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UTBCombatInterface : public UInterface
{
	GENERATED_BODY()
};

class DEVELROCKET_TEAMPROJ_API ITBCombatInterface
{
	GENERATED_BODY()

public:
	// 애니메이션을 실행한 메시의 판정 시작 요청
	virtual void BeginMeleeTrace(USkeletalMeshComponent* SourceMesh) = 0;

	// 애니메이션을 실행한 메시의 판정 종료 요청
	virtual void EndMeleeTrace(USkeletalMeshComponent* SourceMesh) = 0;
};