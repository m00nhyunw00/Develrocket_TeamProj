#include "Components/TBBloodBarComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "UI/TBOverheadBloodBarWidget.h"

UTBBloodBarComponent::UTBBloodBarComponent()
{
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawSize(FVector2D(120.0f, 12.0f));
	SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetWidgetClass(UTBOverheadBloodBarWidget::StaticClass());
	SetVisibility(false);
}

void UTBBloodBarComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() != NM_DedicatedServer && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(VisibilityTimer, this, &ThisClass::UpdateBarVisibility,
			FMath::Max(0.05f, VisibilityCheckInterval), true);
	}
	RefreshBloodBar();
}

void UTBBloodBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(VisibilityTimer);
	UnbindAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UTBBloodBarComponent::BindAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (BoundASC.Get() == InASC && BloodChangedHandle.IsValid() && MaxBloodChangedHandle.IsValid())
	{
		RefreshBloodBar();
		return;
	}
	UnbindAbilitySystem();
	if (!IsValid(InASC) || !InASC->HasAttributeSetForAttribute(UTBAttributeSetBase::GetBloodAttribute())
		|| !InASC->HasAttributeSetForAttribute(UTBAttributeSetBase::GetMaxBloodAttribute())) return;
	BoundASC = InASC;
	BloodChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UTBAttributeSetBase::GetBloodAttribute()).AddUObject(this, &ThisClass::HandleBloodChanged);
	MaxBloodChangedHandle = InASC->GetGameplayAttributeValueChangeDelegate(UTBAttributeSetBase::GetMaxBloodAttribute()).AddUObject(this, &ThisClass::HandleBloodChanged);
	RefreshBloodBar();
}

void UTBBloodBarComponent::UnbindAbilitySystem()
{
	if (UAbilitySystemComponent* ASC = BoundASC.Get())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(UTBAttributeSetBase::GetBloodAttribute()).Remove(BloodChangedHandle);
		ASC->GetGameplayAttributeValueChangeDelegate(UTBAttributeSetBase::GetMaxBloodAttribute()).Remove(MaxBloodChangedHandle);
	}
	BoundASC.Reset();
	BloodChangedHandle.Reset();
	MaxBloodChangedHandle.Reset();
	SetVisibility(false);
}

void UTBBloodBarComponent::HandleBloodChanged(const FOnAttributeChangeData& Data)
{
	RefreshBloodBar();
}

void UTBBloodBarComponent::RefreshBloodBar()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	UAbilitySystemComponent* ASC = BoundASC.Get();
	if (!IsValid(ASC)) { SetVisibility(false); return; }
	InitWidget();
	UTBOverheadBloodBarWidget* BloodWidget = Cast<UTBOverheadBloodBarWidget>(GetUserWidgetObject());
	if (!BloodWidget) { SetVisibility(false); return; }
	const float Blood = ASC->GetNumericAttribute(UTBAttributeSetBase::GetBloodAttribute());
	const float MaxBlood = ASC->GetNumericAttribute(UTBAttributeSetBase::GetMaxBloodAttribute());
	BloodWidget->SetBlood(Blood, MaxBlood);
	UpdateBarVisibility();
}

void UTBBloodBarComponent::UpdateBarVisibility()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	UWorld* World = GetWorld();
	AActor* Target = GetOwner();
	UAbilitySystemComponent* ASC = BoundASC.Get();
	APlayerController* Viewer = World ? World->GetFirstPlayerController() : nullptr;
	if (!World || !IsValid(Target) || Target->IsHidden() || !IsValid(ASC) || !IsValid(Viewer)
		|| ASC->GetNumericAttribute(UTBAttributeSetBase::GetBloodAttribute()) <= 0.0f
		|| ASC->GetNumericAttribute(UTBAttributeSetBase::GetMaxBloodAttribute()) <= 0.0f
		|| !GetUserWidgetObject())
	{
		SetVisibility(false);
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Viewer->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TargetLocation = Target->GetActorLocation();
	if (FVector::DotProduct(ViewRotation.Vector(), TargetLocation - ViewLocation) <= 0.0f)
	{
		SetVisibility(false);
		return;
	}

	FCollisionQueryParams Query(SCENE_QUERY_STAT(BloodBarVisibility), false);
	Query.AddIgnoredActor(Target);
	if (APawn* ViewerPawn = Viewer->GetPawn()) Query.AddIgnoredActor(ViewerPawn);
	const bool bObstructed = World->LineTraceTestByChannel(ViewLocation, TargetLocation, ECC_Visibility, Query);
	SetVisibility(!bObstructed);
}
