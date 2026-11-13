#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TBReplicatedSidewalkSpline.generated.h"

class USplineMeshComponent;
class UStaticMesh;

USTRUCT()
struct FTBReplicatedSidewalkShape
{
	GENERATED_BODY()

	UPROPERTY() TObjectPtr<UStaticMesh> Mesh = nullptr;
	UPROPERTY() FVector Start = FVector::ZeroVector;
	UPROPERTY() FVector StartTangent = FVector::ZeroVector;
	UPROPERTY() FVector End = FVector::ZeroVector;
	UPROPERTY() FVector EndTangent = FVector::ZeroVector;
	UPROPERTY() FVector2D StartScale = FVector2D(1.0, 1.0);
	UPROPERTY() FVector2D EndScale = FVector2D(1.0, 1.0);
	UPROPERTY() uint8 ForwardAxis = 0;
};

// The replicated actor owns the actual walkable spline, giving CharacterMovement
// a stable network reference to its default component.
UCLASS(NotBlueprintable)
class DEVELROCKET_TEAMPROJ_API ATBReplicatedSidewalkSpline : public AActor
{
	GENERATED_BODY()
public:
	ATBReplicatedSidewalkSpline();

	UFUNCTION(BlueprintCallable, Category="City|Sidewalk Collision")
	static void ReplaceWithReplicatedSpline(AActor* Sidewalk, USplineMeshComponent* LocalSpline);

	UFUNCTION(BlueprintCallable, Category="City|Sidewalk Collision")
	static void ClearForSidewalk(AActor* Sidewalk);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION() void OnRep_Shape();
	UFUNCTION() void HandleSidewalkDestroyed(AActor* DestroyedActor);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USplineMeshComponent> SplineMesh;
	UPROPERTY(ReplicatedUsing=OnRep_Shape) FTBReplicatedSidewalkShape Shape;
	TWeakObjectPtr<AActor> SourceSidewalk;
};
