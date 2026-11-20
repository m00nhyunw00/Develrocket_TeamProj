#include "Rendering/TBOutlineSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Components/TBOutlineComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "RHITypes.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/WeakInterfacePtr.h"

UTBOutlineSubsystem::UTBOutlineSubsystem()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(
		TEXT("/Game/_Game/Materials/Outline/M_PP_SkeletalOutline.M_PP_SkeletalOutline"));
	OutlineMaterial = Material.Object;
}

bool UTBOutlineSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

bool UTBOutlineSubsystem::EncompassesPoint(FVector Point, float SphereRadius, float* OutDistanceToPoint)
{
	if (OutDistanceToPoint)
	{
		*OutDistanceToPoint = 0.0f;
	}
	return true;
}

FPostProcessVolumeProperties UTBOutlineSubsystem::GetProperties() const
{
	FPostProcessVolumeProperties Properties{};
	Properties.Settings = &Settings;
	Properties.BlendWeight = 1.0f;
	Properties.bIsEnabled = !Outlines.IsEmpty();
	Properties.bIsUnbound = true;
	Properties.VolumeGuid = VolumeGuid;
	return Properties;
}

bool UTBOutlineSubsystem::InitializePass()
{
	if (OutlineMID)
	{
		return true;
	}
	if (!OutlineMaterial || !FApp::CanEverRender() || GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}

	Styles.Init(FFloat16Color(FLinearColor::Transparent), StencilCount);
	StyleTexture = UTexture2D::CreateTransient(StencilCount, 1, PF_FloatRGBA);
	if (!StyleTexture)
	{
		return false;
	}
	StyleTexture->NeverStream = true;
	StyleTexture->SRGB = false;
	StyleTexture->Filter = TF_Nearest;
	StyleTexture->AddressX = TA_Clamp;
	StyleTexture->AddressY = TA_Clamp;
	StyleTexture->UpdateResource();

	OutlineMID = UMaterialInstanceDynamic::Create(OutlineMaterial, this);
	OutlineMID->SetTextureParameterValue(TEXT("OutlineStyles"), StyleTexture);
	Settings.AddBlendable(OutlineMID.Get(), 1.0f);
	VolumeGuid = FGuid::NewGuid();
	GetWorld()->AddPostProcessVolume(TWeakInterfacePtr<IInterface_PostProcessVolume>(this));
	return true;
}

void UTBOutlineSubsystem::SetOutline(UTBOutlineComponent* Component, UPrimitiveComponent* Mesh, const FTBOutlineStyle& Style)
{
	if (!InitializePass())
	{
		return;
	}

	FOutlineEntry* Entry = Outlines.Find(Component);
	if (Entry && Entry->Mesh.Get() != Mesh)
	{
		RemoveOutline(Component);
		Entry = nullptr;
	}
	if (!Entry)
	{
		for (const auto& Pair : Outlines)
		{
			if (Pair.Value.Mesh.Get() == Mesh)
			{
				return;
			}
		}

		int32 Stencil = FirstOutlineStencil;
		while (Stencil < StencilCount && Styles[Stencil].A.GetFloat() != 0.0f)
		{
			++Stencil;
		}
		if (Stencil == StencilCount)
		{
			UE_LOG(LogTemp, Warning, TEXT("Outline: no free stencil ID for %s"), *GetNameSafe(Mesh));
			return;
		}

		Entry = &Outlines.Add(Component, {Mesh, Stencil, Mesh->CustomDepthStencilValue, Mesh->bRenderCustomDepth != 0});
		Mesh->SetCustomDepthStencilValue(Stencil);
		Mesh->SetRenderCustomDepth(true);
	}

	const float Radius = FMath::Clamp(Style.Thickness, 1, 4);
	FLinearColor PackedStyle = Style.Color.GetClamped(0.0f, 65504.0f);
	PackedStyle.A = Style.bShowThroughWalls ? -Radius : Radius;
	Styles[Entry->Stencil] = FFloat16Color(PackedStyle);
	UploadStyles();
}

void UTBOutlineSubsystem::RemoveOutline(UTBOutlineComponent* Component)
{
	FOutlineEntry Entry;
	if (Outlines.RemoveAndCopyValue(Component, Entry))
	{
		RestoreMesh(Entry);
		Styles[Entry.Stencil] = FFloat16Color(FLinearColor::Transparent);
		UploadStyles();
	}
}

void UTBOutlineSubsystem::RestoreMesh(const FOutlineEntry& Entry)
{
	if (UPrimitiveComponent* Mesh = Entry.Mesh.Get(); Mesh && Mesh->CustomDepthStencilValue == Entry.Stencil)
	{
		Mesh->SetCustomDepthStencilValue(Entry.PreviousStencil);
		Mesh->SetRenderCustomDepth(Entry.bPreviouslyRenderedCustomDepth);
	}
}

void UTBOutlineSubsystem::UploadStyles()
{
	float MaxRadius = 0.0f;
	for (const FFloat16Color& Style : Styles)
	{
		MaxRadius = FMath::Max(MaxRadius, FMath::Abs(Style.A.GetFloat()));
	}
	OutlineMID->SetScalarParameterValue(TEXT("OutlineMaxRadius"), MaxRadius);

	const uint32 Pitch = StencilCount * sizeof(FFloat16Color);
	uint8* Pixels = static_cast<uint8*>(FMemory::Malloc(Pitch));
	FMemory::Memcpy(Pixels, Styles.GetData(), Pitch);
	static const FUpdateTextureRegion2D Region(0, 0, 0, 0, StencilCount, 1);
	StyleTexture->UpdateTextureRegions(0, 1, &Region, Pitch, sizeof(FFloat16Color), Pixels,
		[](uint8* Data, const FUpdateTextureRegion2D*) { FMemory::Free(Data); });
}

void UTBOutlineSubsystem::Deinitialize()
{
	if (OutlineMID)
	{
		GetWorld()->RemovePostProcessVolume(this);
	}
	for (const auto& Pair : Outlines)
	{
		RestoreMesh(Pair.Value);
	}
	Outlines.Reset();
	Settings.WeightedBlendables.Array.Reset();
	OutlineMID = nullptr;
	StyleTexture = nullptr;
	Super::Deinitialize();
}
