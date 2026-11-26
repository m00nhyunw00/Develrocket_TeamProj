#include "UI/HUD/TBDamageIndicatorWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

void UTBDamageIndicatorWidget::AddIndicator(const FVector& Origin, AActor* Source)
{
	if (!IndicatorCanvas)
	{
		return;
	}

	// 같은 공격자면 위치와 시간만 갱신
	if (Source)
	{
		for (FTBDamageIndicatorEntry& Entry : Entries)
		{
			if (Entry.Source.Get() == Source)
			{
				Entry.Origin = Origin;
				Entry.RemainingTime = Duration;
				return;
			}
		}
	}

	// 최대 개수를 넘으면 가장 오래된(남은 시간이 가장 적은) 표시 제거
	if (Entries.Num() >= MaxIndicators)
	{
		int32 OldestIndex = 0;
		for (int32 i = 1; i < Entries.Num(); ++i)
		{
			if (Entries[i].RemainingTime < Entries[OldestIndex].RemainingTime)
			{
				OldestIndex = i;
			}
		}

		if (Entries[OldestIndex].Image)
		{
			Entries[OldestIndex].Image->RemoveFromParent();
		}
		Entries.RemoveAtSwap(OldestIndex);
	}

	FTBDamageIndicatorEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Image = CreateArcImage();
	NewEntry.Source = Source;
	NewEntry.Origin = Origin;
	NewEntry.RemainingTime = Duration;

	// 생성 즉시 올바른 방향으로 보이게 한 번 계산
	if (NewEntry.Image)
	{
		NewEntry.Image->SetRenderTransformAngle(CalcScreenAngle(Origin));
	}
}

UImage* UTBDamageIndicatorWidget::CreateArcImage()
{
	if (!WidgetTree || !IndicatorCanvas)
	{
		return nullptr;
	}

	UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	if (!Image)
	{
		return nullptr;
	}

	if (ArcTexture)
	{
		Image->SetBrushFromTexture(ArcTexture);
	}
	Image->SetColorAndOpacity(ArcColor);
	Image->SetVisibility(ESlateVisibility::HitTestInvisible);

	UCanvasPanelSlot* CanvasSlot = IndicatorCanvas->AddChildToCanvas(Image);
	CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));      // 화면 중앙 기준
	CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	CanvasSlot->SetSize(ArcSize);
	CanvasSlot->SetPosition(FVector2D(0.f, -Radius));  // 중앙에서 위로 Radius만큼

	// 회전 중심을 이미지 중심이 아니라 화면 중앙(아래로 Radius)으로 이동
	const float SafeHeight = FMath::Max(ArcSize.Y, 1.f);
	Image->SetRenderTransformPivot(FVector2D(0.5f, 0.5f + Radius / SafeHeight));

	return Image;
}

float UTBDamageIndicatorWidget::CalcScreenAngle(const FVector& Origin) const
{
	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = GetOwningPlayerPawn();
	if (!PC || !Pawn)
	{
		return 0.f;
	}

	// 1) 나 -> 공격자 방향, Z 제거 후 단위 벡터
	FVector ToSource = Origin - Pawn->GetActorLocation();
	ToSource.Z = 0.f;
	if (!ToSource.Normalize())
	{
		return 0.f;
	}

	// 2) 카메라 Yaw 기준 Forward / Right
	const FRotationMatrix YawMatrix(FRotator(0.f, PC->GetControlRotation().Yaw, 0.f));
	const FVector Forward = YawMatrix.GetUnitAxis(EAxis::X);
	const FVector Right = YawMatrix.GetUnitAxis(EAxis::Y);

	// 3) 내적 두 번 -> 부호 있는 각도 (0=정면, +90=오른쪽, ±180=뒤)
	const float F = FVector::DotProduct(ToSource, Forward);
	const float R = FVector::DotProduct(ToSource, Right);
	return FMath::RadiansToDegrees(FMath::Atan2(R, F));
}

void UTBDamageIndicatorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	for (int32 i = Entries.Num() - 1; i >= 0; --i)
	{
		FTBDamageIndicatorEntry& Entry = Entries[i];
		Entry.RemainingTime -= InDeltaTime;

		if (Entry.RemainingTime <= 0.f || !Entry.Image)
		{
			if (Entry.Image)
			{
				Entry.Image->RemoveFromParent();
			}
			Entries.RemoveAtSwap(i);
			continue;
		}

		// 고개를 돌리면 방향이 따라가도록 매 프레임 재계산
		Entry.Image->SetRenderTransformAngle(CalcScreenAngle(Entry.Origin));
		Entry.Image->SetRenderOpacity(FMath::Clamp(Entry.RemainingTime / Duration, 0.f, 1.f));
	}
}
