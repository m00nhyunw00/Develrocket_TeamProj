#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CityLayoutCoordinator.generated.h"

class UPCGComponent;

USTRUCT(BlueprintType)
struct FReplicatedCityLayout
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="City") int32 Seed = 0;
    UPROPERTY(BlueprintReadOnly, Category="City") int32 Revision = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCityLayoutReady, int32, Seed, int32, Revision);

// Replicates the layout identity; each machine generates its own non-replicated city.
UCLASS()
class DEVELROCKET_TEAMPROJ_API ACityLayoutCoordinator : public AActor
{
    GENERATED_BODY()
public:
    ACityLayoutCoordinator();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="City")
    TObjectPtr<AActor> CityVolume;

    UPROPERTY(ReplicatedUsing=OnRep_Layout, VisibleInstanceOnly, BlueprintReadOnly, Category="City")
    FReplicatedCityLayout Layout;

    UPROPERTY(BlueprintAssignable, Category="City") FCityLayoutReady OnLocalCityReady;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="City") bool bLocalCityReady = false;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="City") void GenerateNewCity();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient) TObjectPtr<UPCGComponent> CityPCG;
    int32 GeneratingRevision = 0;
    int32 CompletedRevision = 0;
    bool bInitialized = false;

    UFUNCTION() void OnRep_Layout();
    UFUNCTION() void OnGenerated(UPCGComponent* Component);
    UFUNCTION() void OnCancelled(UPCGComponent* Component);
    void ApplyLatestLayout();
};
