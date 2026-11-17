#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TBCombatInterface.generated.h"

class USkeletalMeshComponent;

// 캐릭터의 C++ 구현을 통해 사용하는 공통 전투 요청
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UTBCombatInterface : public UInterface
{
	GENERATED_BODY()
};

class DEVELROCKET_TEAMPROJ_API ITBCombatInterface
{
	GENERATED_BODY()

public:
	// 지정한 설정으로 새로운 근접 판정 구간 시작
	virtual bool BeginMeleeTrace(FName ProfileName, USkeletalMeshComponent* SourceMesh, const UObject* WindowOwner, int32 MontageInstanceID) = 0;

	// 시작 요청과 일치하는 근접 판정 구간 종료
	virtual void EndMeleeTrace(USkeletalMeshComponent* SourceMesh, const UObject* WindowOwner, int32 MontageInstanceID) = 0;
};