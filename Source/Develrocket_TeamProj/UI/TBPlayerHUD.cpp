#include "UI/TBPlayerHUD.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/TBAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/TBAttributeSetBase.h"
#include "AbilitySystem/Attributes/TBPlayerAttributeSet.h"
#include "Blueprint/UserWidget.h"
#include "Framework/TBPlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "UI/Menu/TBPauseMenuWidget.h"
#include "UI/HUD/TBPlayerHUDWidget.h"
#include "UI/Menu/TBMatchWaitWidget.h"
#include "Framework/TBLevelManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/TBInteractionComponent.h"
#include "Interface/TBInteractInterface.h"
#include "UI/Menu/TBLobbyReadyWidget.h"
#include "UI/Menu/TBClassSelectWidget.h"
#include "Framework/TBLobbyManager.h"
#include "InGame/TBLeaderboardWidget.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "TBGameplayTags.h"
#include "TimerManager.h"

namespace
{
    constexpr float SkillCooldownRefreshInterval = 0.1f;
}

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

    // 직업 선택 : 로비에 들어오면 항상 먼저 띄움
    // (입장 직후 클라엔 LobbyManager 목록 복제가 아직 없을 수 있어 "미선택일 때만" 조건은 쓰지 않음)
    if (ClassSelectWidgetClass && UGameplayStatics::GetActorOfClass(this, ATBLobbyManager::StaticClass()))
    {
        ClassSelectWidget = CreateWidget<UTBClassSelectWidget>(PC, ClassSelectWidgetClass);
        if (ClassSelectWidget)
        {
            ClassSelectWidget->AddToViewport(15);
            ClassSelectWidget->OpenSelect();
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
    
    if (UGameplayStatics::GetActorOfClass(this, ATBLevelManager::StaticClass()))
    {
        LeaderboardWidget = CreateWidget<UTBLeaderboardWidget>(
            PC, LeaderboardWidgetClass ? LeaderboardWidgetClass.Get() : UTBLeaderboardWidget::StaticClass());
        if (LeaderboardWidget)
        {
            LeaderboardWidget->AddToViewport(8);
            LeaderboardWidget->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

TArray<FTBLeaderboardRowData> ATBPlayerHUD::BuildLeaderboardRows() const
{
    TArray<FTBLeaderboardRowData> Rows;
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if (!GS) return Rows;

    // HUD를 소유한 로컬 PC의 PlayerState = "나"
    const APlayerController* OwningPC = GetOwningPlayerController();
    const APlayerState* LocalPS = OwningPC ? OwningPC->PlayerState.Get() : nullptr;

    for (const APlayerState* PS : GS->PlayerArray)
    {
        if (!IsValid(PS) || PS->IsInactive()) continue; // 접속이 끊긴 플레이어 제외

        FTBLeaderboardRowData& Row = Rows.AddDefaulted_GetRef();
        Row.PlayerName = FText::FromString(PS->GetPlayerName());
        Row.PingMs = FMath::RoundToInt(PS->GetPingInMilliseconds());
        Row.bIsLocalPlayer = (PS == LocalPS);

        if (const ATBPlayerState* TBPlayerState = Cast<ATBPlayerState>(PS))
        {
            const FTBMatchStats& Stats = TBPlayerState->GetMatchStats();
            Row.Kills = Stats.Kills;
            Row.Deaths = Stats.Deaths;
            Row.Score = Stats.MatchScore;
        }
    }

    // 점수와 킬이 같으면 사망 수가 적은 플레이어를 먼저 표시
    Rows.StableSort([](const FTBLeaderboardRowData& A, const FTBLeaderboardRowData& B)
    {
        if (A.Score != B.Score)
        {
            return A.Score > B.Score;
        }
        if (A.Kills != B.Kills)
        {
            return A.Kills > B.Kills;
        }
        return A.Deaths < B.Deaths;
    });

    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        Rows[Index].Rank = Index + 1;
    }

    return Rows;
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
        RefreshSkillCooldownHUD();
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

    const FGameplayTag SkillCooldownTags[] = {TBGameplayTags::Cooldown_Skill1, TBGameplayTags::Cooldown_Skill2};
    for (const FGameplayTag& Tag : SkillCooldownTags)
    {
        const FDelegateHandle Handle = InASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
            .AddUObject(this, &ThisClass::HandleSkillCooldownTagChanged);
        SkillCooldownBindings.Emplace(Tag, Handle);
    }
    RefreshSkillCooldownHUD();

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

void ATBPlayerHUD::HandleSkillCooldownTagChanged(const FGameplayTag, int32)
{
    RefreshSkillCooldownHUD();
}

void ATBPlayerHUD::RefreshSkillCooldownHUD()
{
    UAbilitySystemComponent* ASC = BoundASC.Get();
    if (!ASC || !HUDWidget)
    {
        GetWorldTimerManager().ClearTimer(SkillCooldownRefreshTimer);
        return;
    }

    bool bHasActiveCooldown = false;
    for (int32 Index = 0; Index < SkillCooldownBindings.Num(); ++Index)
    {
        const FGameplayTag Tag = SkillCooldownBindings[Index].Key;
        float Remaining = 0.0f;
        if (ASC->HasMatchingGameplayTag(Tag))
        {
            bHasActiveCooldown = true;
            const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(Tag));
            for (const float EffectRemaining : ASC->GetActiveEffectsTimeRemaining(Query))
            {
                Remaining = FMath::Max(Remaining, EffectRemaining);
            }
        }
        HUDWidget->SetSkillCooldownRemaining(Index, Remaining);
    }

    FTimerManager& Timers = GetWorldTimerManager();
    if (!bHasActiveCooldown)
    {
        Timers.ClearTimer(SkillCooldownRefreshTimer);
    }
    else if (!Timers.IsTimerActive(SkillCooldownRefreshTimer))
    {
        Timers.SetTimer(SkillCooldownRefreshTimer, this, &ThisClass::RefreshSkillCooldownHUD, SkillCooldownRefreshInterval, true);
    }
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

void ATBPlayerHUD::ShowDamageIndicator(const FVector& Origin, AActor* Source)
{
    if (HUDWidget)
    {
        HUDWidget->AddDamageIndicator(Origin, Source);
    }
}

void ATBPlayerHUD::HandleFocusedActorChanged(AActor* NewFocus)
{
    if (!HUDWidget)
    {
        return;
    }
    
    if (NewFocus && NewFocus->Implements<UTBInteractInterface>())
    {
        const FText Prompt = ITBInteractInterface::Execute_GetInteractPrompt(NewFocus);
        HUDWidget->ShowInteractPrompt(FText::FromString(TEXT("C")), Prompt);
    }
    else
    {
        HUDWidget->HideInteractPrompt();
    }
}

void ATBPlayerHUD::RefreshLeaderboard()
{
    if (LeaderboardWidget) LeaderboardWidget->SetLeaderboardRows(BuildLeaderboardRows());
}

void ATBPlayerHUD::UnbindAbilitySystem()
{
    if (UAbilitySystemComponent* ASC = BoundASC.Get())
    {
        for (const auto& Binding : AttributeBindings)
        {
            ASC->GetGameplayAttributeValueChangeDelegate(Binding.Key).Remove(Binding.Value);
        }
        for (const auto& Binding : SkillCooldownBindings)
        {
            ASC->UnregisterGameplayTagEvent(Binding.Value, Binding.Key, EGameplayTagEventType::NewOrRemoved);
        }

        if (UTBAbilitySystemComponent* TBASC = Cast<UTBAbilitySystemComponent>(ASC))
        {
            TBASC->OnPrimaryShotFired.Remove(ShotFiredHandle);
        }
    }

    GetWorldTimerManager().ClearTimer(SkillCooldownRefreshTimer);
    if (HUDWidget)
    {
        for (int32 Index = 0; Index < SkillCooldownBindings.Num(); ++Index)
        {
            HUDWidget->SetSkillCooldownRemaining(Index, 0.0f);
        }
    }
    SkillCooldownBindings.Reset();
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
        HideLeaderboard();
        PauseWidget->OpenPauseMenu();
    }
}

void ATBPlayerHUD::OpenClassSelect()
{
    if (!ClassSelectWidget)
    {
        return;
    }

    HideLeaderboard();
    ClassSelectWidget->OpenSelect();
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

void ATBPlayerHUD::ShowLeaderboard()
{
    if (!LeaderboardWidget) return;
    RefreshLeaderboard();
    LeaderboardWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
    GetWorldTimerManager().SetTimer(LeaderboardRefreshTimer, this, &ThisClass::RefreshLeaderboard, LeaderboardRefreshInterval, true);
}

void ATBPlayerHUD::HideLeaderboard()
{
    GetWorldTimerManager().ClearTimer(LeaderboardRefreshTimer);
    if (LeaderboardWidget) LeaderboardWidget->SetVisibility(ESlateVisibility::Collapsed);
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
    
    if (LeaderboardWidget)
    {
        GetWorldTimerManager().ClearTimer(LeaderboardRefreshTimer);
        LeaderboardWidget->RemoveFromParent();
    }

    Super::EndPlay(EndPlayReason);
}
