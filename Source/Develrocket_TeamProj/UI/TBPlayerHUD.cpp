#include "UI/TBPlayerHUD.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "AbilitySystem/Attributes/TBPlayerAttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "Framework/TBPlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/TBPauseMenuWidget.h"
#include "UI/TBPlayerHUDWidget.h"
#include "UI/TBMatchWaitWidget.h"
#include "Framework/TBLevelManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/TBInteractionComponent.h"
#include "Interface/TBInteractable.h"
#include "UI/TBLobbyReadyWidget.h"
#include "Framework/TBLobbyManager.h"

void ATBPlayerHUD::BeginPlay()
{
    Super::BeginPlay();

    APlayerController* PC = GetOwningPlayerController();

    if (!PC || !PC->IsLocalController())
    {
        return;
    }

    // HUD 생성
    if (HUDWidgetClass)
    {
        HUDWidget = CreateWidget<UTBPlayerHUDWidget>(PC, HUDWidgetClass);

        if (HUDWidget)
        {
            HUDWidget->AddToViewport();
            
            // 슬롯별 표시 키 설정
            HUDWidget->SetSkill(0, nullptr, FText::FromString(TEXT("Q")));
            HUDWidget->SetSkill(1, nullptr, FText::FromString(TEXT("E")));
        }
    }

    // 일시정지 메뉴 생성
    if (PauseWidgetClass)
    {
        PauseWidget = CreateWidget<UTBPauseMenuWidget>(PC, PauseWidgetClass);

        if (PauseWidget)
        {
            PauseWidget->AddToViewport(10);
            PauseWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    
    // 대기 화면 : LevelManager가 있는 맵 안에서만
    if (MatchWaitWidgetClass && UGameplayStatics::GetActorOfClass(this, ATBLevelManager::StaticClass()))
    {
        if (UTBMatchWaitWidget* WaitWidget = CreateWidget<UTBMatchWaitWidget>(PC, MatchWaitWidgetClass))
        {
            WaitWidget->AddToViewport(20);
        }
    }
    
    // 준비 목록 : LobbyManager가 있는 맵 안에서만
    if (LobbyReadyWidgetClass && UGameplayStatics::GetActorOfClass(this, ATBLobbyManager::StaticClass()))
    {
        if (UTBLobbyReadyWidget* ReadyWidget = CreateWidget<UTBLobbyReadyWidget>(PC, LobbyReadyWidgetClass))
        {
            ReadyWidget->AddToViewport(5);
        }
    }

    // 이미 준비된 PlayerState가 있으면 GAS 연결
    ATBPlayerState* PS = PC->GetPlayerState<ATBPlayerState>();

    if (!PS && PC->GetPawn())
    {
        PS = PC->GetPawn()->GetPlayerState<ATBPlayerState>();
    }

    if (PS)
    {
        BindAbilitySystem(PS->GetAbilitySystemComponent());
    }
}

void ATBPlayerHUD::BindAbilitySystem(UAbilitySystemComponent* InASC)
{
    if (!HUDWidget || !IsValid(InASC))
    {
        return;
    }

    if (!InASC->GetSet<UTBPlayerAttributeSet>())
    {
        return;
    }

    // 같은 ASC에 중복 연결 방지
    if (BoundASC.Get() == InASC)
    {
        RefreshResourceHUD();
        return;
    }

    UnbindAbilitySystem();
    BoundASC = InASC;
    
    // 체력 및 탄약 속성 구독
    const FGameplayAttribute Attributes[] =
    {
        // 공통 체력 속성
        UTBAttributeSetBase::GetBloodAttribute(),
        UTBAttributeSetBase::GetMaxBloodAttribute(),

        // 플레이어 전용 탄약 속성
        UTBPlayerAttributeSet::GetLoadedBloodAttribute(),
        UTBPlayerAttributeSet::GetMaxLoadedBloodAttribute(),
        UTBPlayerAttributeSet::GetAmmoAtLastReloadAttribute()
    };

    for (const FGameplayAttribute& Attribute : Attributes)
    {
        FDelegateHandle Handle = InASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ATBPlayerHUD::HandleAttributeChanged);

        AttributeBindings.Emplace(Attribute, Handle);
    }

    // 기본 공격 발사 이벤트 구독
    if (UTBAbilitySystemComponent* ASC = Cast<UTBAbilitySystemComponent>(InASC))
    {
        ShotFiredHandle = ASC->OnPrimaryShotFired.AddUObject(this, &ATBPlayerHUD::NotifyShotFired);
    }

    // 연결 직후 현재 값 표시
    RefreshResourceHUD();
}

void ATBPlayerHUD::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
    RefreshResourceHUD();
}

void ATBPlayerHUD::RefreshResourceHUD()
{
    UAbilitySystemComponent* ASC = BoundASC.Get();

    if (!ASC || !HUDWidget)
    {
        return;
    }

    const UTBPlayerAttributeSet* Attributes = ASC->GetSet<UTBPlayerAttributeSet>();

    if (!Attributes)
    {
        return;
    }

    // 실제 체력 표시
    HUDWidget->SetHealth(Attributes->GetBlood(), Attributes->GetMaxBlood());

    // 실제 재장전 가능 여부
    const bool bCanReload = Attributes->GetBlood() > KINDA_SMALL_NUMBER &&Attributes->GetLoadedBlood() < Attributes->GetMaxLoadedBlood();

    // 현재 잔탄 / 마지막 재장전 완료 당시 잔탄
    HUDWidget->SetAmmo(
FMath::RoundToInt(Attributes->GetLoadedBlood()),
        FMath::RoundToInt(Attributes->GetAmmoAtLastReload()),
        FMath::RoundToInt(Attributes->GetMaxLoadedBlood()),
        bCanReload);
}

void ATBPlayerHUD::NotifyShotFired()
{
    if (HUDWidget)
    {
        HUDWidget->NotifyShotFired();
    }
}

void ATBPlayerHUD::HandleFocusedActorChanged(AActor* NewFocus)
{
    if (!HUDWidget)
    {
        return;
    }
    
    if (NewFocus && NewFocus->Implements<UTBInteractable>())
    {
        const FText Prompt = ITBInteractable::Execute_GetInteractPrompt(NewFocus);
        HUDWidget->ShowInteractPrompt(FText::FromString(TEXT("F")), Prompt);
    }
    else
    {
        HUDWidget->HideInteractPrompt();
    }
}

void ATBPlayerHUD::UnbindAbilitySystem()
{
    if (UAbilitySystemComponent* ASC = BoundASC.Get())
    {
        for (const auto& Binding : AttributeBindings)
        {
            ASC->GetGameplayAttributeValueChangeDelegate(Binding.Key).Remove(Binding.Value);
        }

        if (UTBAbilitySystemComponent* TBASC = Cast<UTBAbilitySystemComponent>(ASC))
        {
            TBASC->OnPrimaryShotFired.Remove(ShotFiredHandle);
        }
    }

    AttributeBindings.Reset();
    ShotFiredHandle.Reset();
    BoundASC.Reset();
}

void ATBPlayerHUD::TogglePauseMenu()
{
    if (!PauseWidget)
    {
        return;
    }

    if (PauseWidget->IsVisible())
    {
        PauseWidget->ClosePauseMenu();
    }
    else
    {
        PauseWidget->OpenPauseMenu();
    }
}

void ATBPlayerHUD::BindInteraction(UTBInteractionComponent* InInteraction)
{
    if (!HUDWidget || !IsValid(InInteraction) || BoundInteraction.Get() == InInteraction)
    {
        return;
    }
    
    UnBindInteraction();
    BoundInteraction = InInteraction;
    
    FocusChangedHandle = InInteraction->OnFocusedActorChanged.AddUObject(this, &ATBPlayerHUD::HandleFocusedActorChanged);
    
    HandleFocusedActorChanged(InInteraction->GetFocusedActor());
}

void ATBPlayerHUD::UnBindInteraction()
{
    if (UTBInteractionComponent* Interaction = BoundInteraction.Get())
    {
        Interaction->OnFocusedActorChanged.Remove(FocusChangedHandle);
    }
    
    FocusChangedHandle.Reset();
    BoundInteraction.Reset();
    
    if (HUDWidget)
    {
        HUDWidget->HideInteractPrompt();
    }
}



void ATBPlayerHUD::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    UnbindAbilitySystem();
    UnBindInteraction();

    if (HUDWidget)
    {
        HUDWidget->RemoveFromParent();
    }

    if (PauseWidget)
    {
        PauseWidget->RemoveFromParent();
    }

    Super::EndPlay(EndPlayReason);
}
