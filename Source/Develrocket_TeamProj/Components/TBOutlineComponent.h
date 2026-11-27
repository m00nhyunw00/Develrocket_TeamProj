#pragma once

#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "TBOutlineComponent.generated.h"

USTRUCT(BlueprintType)
struct FTBOutlineStyle
{
	GENERATED_BODY()

	/** Radius in Custom Stencil buffer pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outline", meta = (ClampMin = "1", ClampMax = "4", UIMin = "1", UIMax = "4"))
	int32 Thickness = 1;

	/** Linear HDR color. RGB values above 1 can contribute to Bloom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outline", meta = (HideAlphaChannel))
	FLinearColor Color = FLinearColor(0.15f, 1.0f, 3.0f);
};

/** Adds a post-process outline to one skeletal or static mesh on this actor. */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent, DisplayName = "Outline Component"))
class DEVELROCKET_TEAMPROJ_API UTBOutlineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void SetOutlineSuppressed(bool bSuppressed);
	bool CanRevealWithBloodPulse() const;
	void RevealWithBloodPulse(float Duration);
	void ClearBloodPulseReveal();

	UFUNCTION(BlueprintCallable, Category = "Outline")
	void SetOutlineStyle(const FTBOutlineStyle& NewStyle);

	// Linear RGB. Values above 1 are supported for HDR outlines.
	UFUNCTION(BlueprintCallable, Category = "Outline")
	virtual void SetOutlineColor(float Red, float Green, float Blue);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif

private:
	/** Leave empty to prefer a skeletal mesh, then fall back to a static mesh. */
	UPROPERTY(EditAnywhere, Category = "Outline", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SkeletalMeshComponent,/Script/Engine.StaticMeshComponent"))
	FComponentReference TargetMesh;

	UPROPERTY(EditAnywhere, Category = "Outline", meta = (DisplayName = "Detectable by Blood Pulse"))
	bool bDetectableByBloodPulse = false;

	UPROPERTY(EditAnywhere, Category = "Outline", meta = (ShowOnlyInnerProperties))
	FTBOutlineStyle Style;

	bool bOutlineSuppressed = false;
	FTimerHandle BloodPulseRevealTimer;

	void RefreshOutline();
};
