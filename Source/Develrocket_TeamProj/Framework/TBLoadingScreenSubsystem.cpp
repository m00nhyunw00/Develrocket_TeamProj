#include "Framework/TBLoadingScreenSubsystem.h"
#include "MoviePlayer.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"

void UTBLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadingWidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/_Game/Blueprints/UI/Loading/WBP_HostSpinner.WBP_HostSpinner_C"));
	SpinnerWidgetClass = LoadingWidgetClass;
	FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &UTBLoadingScreenSubsystem::OnPreLoadMap);
}

void UTBLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	Super::Deinitialize();
}

void UTBLoadingScreenSubsystem::OnPreLoadMap(const FString& Mapname)
{
	if (IsRunningDedicatedServer()) return;
	
	const FString ShortName = FPackageName::GetShortName(UWorld::RemovePIEPrefix(Mapname));
	UE_LOG(LogTemp, Log, TEXT("[TBLoading] PreLoadMap: %s -> %s"), *Mapname, *ShortName);
	
	TSubclassOf<UUserWidget> ClassToShow = nullptr;
	float MinDisplayTime = 0.f;
	
	if (ShortName == LoadingScreenMapName)
	{
		ClassToShow = LoadingWidgetClass;
		MinDisplayTime = 2.0f;
	}
	else if (ShortName == SpinnerMapName)
	{
		ClassToShow = SpinnerWidgetClass;
		MinDisplayTime = 0.5f;
	}
	
	if (!ClassToShow) return;
	
	UUserWidget* Widget = CreateWidget<UUserWidget>(GetGameInstance(), ClassToShow);
	if (!Widget) return;
	
	FLoadingScreenAttributes Attr;
	Attr.bAutoCompleteWhenLoadingCompletes = true;
	Attr.MinimumLoadingScreenDisplayTime = MinDisplayTime;
	Attr.WidgetLoadingScreen = Widget->TakeWidget();
	GetMoviePlayer()->SetupLoadingScreen(Attr);
}
