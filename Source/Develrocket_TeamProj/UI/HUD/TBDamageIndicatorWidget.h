#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TBDamageIndicatorWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTexture2D;

USTRUCT()
struct FTBDamageIndicatorEntry
{
	GENERATED_BODY()

	// 화면에 그려지는 호 이미지
	UPROPERTY()
	TObjectPtr<UImage> Image;

	// 같은 공격자에게 다시 맞으면 새로 만들지 않고 갱신하기 위한 키
	TWeakObjectPtr<AActor> Source;

	// 서버가 알려준 공격 원점 (월드 좌표)
	FVector Origin = FVector::ZeroVector;

	float RemainingTime = 0.f;
};

/**
 * 피격 방향 표시
 * 서버가 보낸 공격 원점을 보관하고, 매 Tick 카메라 기준 각도를 다시 계산해 크로스헤어 주변 호를 회전시킨다.
 */
UCLASS()
class DEVELROCKET_TEAMPROJ_API UTBDamageIndicatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void AddIndicator(const FVector& Origin, AActor* Source);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP 루트에 같은 이름의 Canvas Panel 필요
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UCanvasPanel> IndicatorCanvas;

	// 위쪽을 향한 호 모양 텍스처 (비워두면 빨간 사각형으로 표시)
	UPROPERTY(EditDefaultsOnly, Category="UI")
	TObjectPtr<UTexture2D> ArcTexture;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	FVector2D ArcSize = FVector2D(160.f, 40.f);

	// 화면 중앙에서 호까지의 거리
	UPROPERTY(EditDefaultsOnly, Category="UI")
	float Radius = 180.f;

	UPROPERTY(EditDefaultsOnly, Category="UI", meta=(ClampMin="0.1", Units="s"))
	float Duration = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category="UI")
	FLinearColor ArcColor = FLinearColor(0.8f, 0.05f, 0.05f, 0.85f);

	UPROPERTY(EditDefaultsOnly, Category="UI", meta=(ClampMin="1"))
	int32 MaxIndicators = 6;

private:
	UImage* CreateArcImage();
	float CalcScreenAngle(const FVector& Origin) const;

	UPROPERTY(Transient)
	TArray<FTBDamageIndicatorEntry> Entries;
};
