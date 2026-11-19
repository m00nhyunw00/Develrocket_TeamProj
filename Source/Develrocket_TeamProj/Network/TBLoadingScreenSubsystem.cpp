#include "TBLoadingScreenSubsystem.h"
#include "MoviePlayer.h"
#include "Blueprint/UserWidget.h"

void UTBLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadingWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Developers/Dev_Choi/UI/Loading/WBP_Loading.WBP_Loading_C"));
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UTBLoadingScreenSubsystem::OnPreLoadMap);
}

void UTBLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	Super::Deinitialize();
}

void UTBLoadingScreenSubsystem::OnPreLoadMap(const FString& Mapname)
{
	if (IsRunningDedicatedServer() || !LoadingWidgetClass) return;
	
	UUserWidget* Widget = CreateWidget<UUserWidget>(GetGameInstance(), LoadingWidgetClass);
	if (!Widget) return;
	
	FLoadingScreenAttributes Attr;
	Attr.bAutoCompleteWhenLoadingCompletes = true;
	Attr.MinimumLoadingScreenDisplayTime = 2.0f;
	Attr.WidgetLoadingScreen = Widget->TakeWidget();
	GetMoviePlayer()->SetupLoadingScreen(Attr);
}
