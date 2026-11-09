#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBSidewalkCollision.generated.h"

class UBoxComponent;

// 서버가 생성하고 클라이언트에 복제하는 보도 구간 충돌
UCLASS(NotBlueprintable)
class DEVELROCKET_TEAMPROJ_API ATBSidewalkCollision : public AActor
{
	GENERATED_BODY()

public:
	ATBSidewalkCollision();

	// 로컬 생성 Box의 최종 월드 변환과 크기로 서버 전용 복제 충돌 생성
	UFUNCTION(BlueprintCallable, Category="City|Sidewalk Collision")
	static void CreateForBox(AActor* Sidewalk, UBoxComponent* SourceBox);

	// 같은 보도를 다시 만들 때 이전 충돌 액터 제거
	UFUNCTION(BlueprintCallable, Category="City|Sidewalk Collision")
	static void ClearForSidewalk(AActor* Sidewalk);

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_BoxExtent();

	UFUNCTION()
	void HandleSidewalkDestroyed(AActor* DestroyedActor);

	UPROPERTY(VisibleAnywhere, Category="Collision")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(ReplicatedUsing=OnRep_BoxExtent)
	FVector BoxExtent = FVector::ZeroVector;

	TWeakObjectPtr<AActor> SourceSidewalk;
};
