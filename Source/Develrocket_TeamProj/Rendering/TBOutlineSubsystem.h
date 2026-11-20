#pragma once

#include "Engine/Scene.h"
#include "Interfaces/Interface_PostProcessVolume.h"
#include "Math/Float16Color.h"
#include "Subsystems/WorldSubsystem.h"
#include "TBOutlineSubsystem.generated.h"

class UTBOutlineComponent;
class UPrimitiveComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTexture2D;
struct FTBOutlineStyle;

/** Maintains one post-process pass and a stencil-indexed style texture per world. */
UCLASS()
class UTBOutlineSubsystem : public UWorldSubsystem, public IInterface_PostProcessVolume
{
	GENERATED_BODY()

public:
	UTBOutlineSubsystem();
	void SetOutline(UTBOutlineComponent* Component, UPrimitiveComponent* Mesh, const FTBOutlineStyle& Style);
	void RemoveOutline(UTBOutlineComponent* Component);
	virtual void Deinitialize() override;

	virtual bool EncompassesPoint(FVector Point, float SphereRadius, float* OutDistanceToPoint) override;
	virtual FPostProcessVolumeProperties GetProperties() const override;
#if DEBUG_POST_PROCESS_VOLUME_ENABLE
	virtual FString GetDebugName() const override { return TEXT("Outline"); }
#endif

protected:
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	static constexpr int32 StencilCount = 256;
	// Reserve the high half of the stencil range for this effect.
	static constexpr int32 FirstOutlineStencil = 128;

	struct FOutlineEntry
	{
		TWeakObjectPtr<UPrimitiveComponent> Mesh;
		int32 Stencil = 0;
		int32 PreviousStencil = 0;
		bool bPreviouslyRenderedCustomDepth = false;
	};

	bool InitializePass();
	void UploadStyles();
	void RestoreMesh(const FOutlineEntry& Entry);

	UPROPERTY()
	TObjectPtr<UMaterialInterface> OutlineMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OutlineMID;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> StyleTexture;

	UPROPERTY(Transient)
	FPostProcessSettings Settings;

	TMap<const UTBOutlineComponent*, FOutlineEntry> Outlines;
	TArray<FFloat16Color> Styles;
	FGuid VolumeGuid;
};
