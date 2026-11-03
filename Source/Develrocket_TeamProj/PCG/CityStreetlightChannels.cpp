#include "PCG/CityStreetlightChannels.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGElement.h"

class FCityStreetlightChannelsElement : public IPCGElement
{
public:
    virtual bool CanExecuteOnlyOnMainThread(FPCGContext*) const override { return true; }
    virtual bool IsCacheable(const UPCGSettings*) const override { return false; }
protected:
    virtual bool SupportsBasePointDataInputs(FPCGContext*) const override { return true; }
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        const UPCGComponent* Source = Cast<UPCGComponent>(Context->ExecutionSource.GetObject());
        AActor* Owner = Source ? Source->GetOwner() : nullptr;
        if (Owner)
        {
            TInlineComponentArray<UInstancedStaticMeshComponent*> Components(Owner);
            for (UInstancedStaticMeshComponent* Component : Components)
            {
                const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
                if (Mesh && Mesh->GetPathName() == TEXT("/Game/_Assets/_Original/VictorianStreet/Meshes/Static/sm_streetlight_a.sm_streetlight_a"))
                {
                    // PCG 스포너가 상위 액터의 Static Mobility를 복사하므로 생성 후 가로등만 보정
                    Component->SetMobility(EComponentMobility::Movable);
                    Component->SetLightingChannels(false, true, false);
                }
            }
        }
        for (const FPCGTaggedData& Tagged : Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel))
        {
            FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef(Tagged);
            Output.Pin = PCGPinConstants::DefaultOutputLabel;
        }
        return true;
    }
};

FPCGElementPtr UCityStreetlightChannelsSettings::CreateElement() const
{
    return MakeShared<FCityStreetlightChannelsElement>();
}
