#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PCGSettings.h"
#include "CityPacking.generated.h"

class UPCGComponent;

USTRUCT(BlueprintType)
// 건물 클래스와 자식 액터를 포함한 로컬 경계 정보
struct FCityBuildingFootprint
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building") TSubclassOf<AActor> BuildingClass;
    // Measured bounds of the complete BP, including its child actors, in unscaled local space.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building") FVector BoundsMin = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building") FVector BoundsMax = FVector::ZeroVector;
};

USTRUCT()
// 서버에서 동기화하는 배치 시드와 생성 버전
struct FCityLayoutState
{
    GENERATED_BODY()
    
    UPROPERTY()
    int32 Seed = 42;
    
    UPROPERTY()
    int32 Revision = 0;
};

UCLASS(BlueprintType)
// Floor 영역과 시드를 관리하는 도시 패킹 액터
class DEVELROCKET_TEAMPROJ_API ACityPackingActor : public AActor
{
    GENERATED_BODY()
public:
    ACityPackingActor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City") TObjectPtr<UPCGComponent> PCGComponent;
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="City") TObjectPtr<AActor> Floor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City", meta=(ClampMin="0")) double BuildingGap = 250;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City", meta=(ClampMin="0")) double EdgeMargin = 250;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City", meta=(ClampMin="1", ClampMax="256")) int32 MaxBuildings = 96;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City") int32 LayoutSeed = 42;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="City") bool bRandomizeOnPlay = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Result") int32 LastBuildingCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Result") int32 LastLayoutSeed = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="City|Result") double LastCoveragePercent = 0;
    UFUNCTION(BlueprintCallable, CallInEditor, Category="City") void NewLayout();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="City") void RebuildSameSeed();
    // 동일 시드 재생성 요청 상태; 새 시드 자동 변경 로직은 현재 비활성화
    bool bReuseSeedNextGeneration = false;
    
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void GenerateNewLayoutOnServer();
protected:
    virtual void BeginPlay() override;
    
    UPROPERTY(ReplicatedUsing = OnRep_LayoutState)
    FCityLayoutState LayoutState;
    
    UFUNCTION()
    void OnRep_LayoutState();
    
    void ApplyLayoutState();
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
// Floor 전체를 대상으로 건물 배치 포인트를 계산하는 PCG 노드
class DEVELROCKET_TEAMPROJ_API UCityPackingSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings") TArray<FCityBuildingFootprint> Buildings;
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("CityFootprintPacking"); }
    virtual FText GetDefaultNodeTitle() const override { return FText::FromString(TEXT("City Footprint Packing")); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif
protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }
    virtual FPCGElementPtr CreateElement() const override;
};
