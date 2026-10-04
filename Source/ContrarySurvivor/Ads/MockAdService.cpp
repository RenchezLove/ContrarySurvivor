// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Ads/MockAdService.h"
#include "ContrarySurvivor/Ads/MockAdWidget.h"
#include "ContrarySurvivor/Ads/MockBannerWidget.h"
#include "ContrarySurvivor/Ads/YandexAdService.h"
#include "ContrarySurvivor/Ads/ContraryAdSettings.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h" // слот MockAdWidgetClass (ТЗ 08-07)
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UMockAdService::ShowRewarded(FName Placement, FSimpleDelegate OnSuccess, FSimpleDelegate OnFail)
{
	// Один «ролик» за раз: повторный вызов до закрытия — отказ показа, награды нет.
	if (ActiveWidget)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: MOCK-AD '%s' rejected - another ad is already showing"),
			*Placement.ToString());
		OnFail.ExecuteIfBound();
		return;
	}

	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: MOCK-AD '%s' rejected - no local player controller"),
			*Placement.ToString());
		OnFail.ExecuteIfBound();
		return;
	}

	// ТЗ Рината 08-07: слот WBP-класса на HUD (ADR-048). Назначен — экран-заглушка живёт
	// на дереве владельца из дизайнера; пуст — прежний кодовый вид.
	UClass* AdWidgetClass = UMockAdWidget::StaticClass();
	if (const AContrarySurvivorHUD* Hud = Cast<AContrarySurvivorHUD>(PC->GetHUD()))
	{
		if (Hud->MockAdWidgetClass)
		{
			AdWidgetClass = Hud->MockAdWidgetClass;
		}
	}
	ActiveWidget = CreateWidget<UMockAdWidget>(PC, AdWidgetClass);
	if (!ActiveWidget)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: MOCK-AD '%s' rejected - widget creation failed"),
			*Placement.ToString());
		OnFail.ExecuteIfBound();
		return;
	}

	PendingSuccess = OnSuccess;
	bActiveIsInterstitial = false;

	ActiveWidget->SetPlacement(Placement);
	ActiveWidget->OnClosed.AddUObject(this, &UMockAdService::HandleAdClosed);
	// Поверх любых модалок (экран смерти/магазин живут на ZOrder <= 100).
	ActiveWidget->AddToViewport(/*ZOrder=*/300);

	// Пауза на время «ролика» (ТЗ раздел 0 п.7). Прежнее состояние запоминаем и вернём.
	bWasPausedBefore = UGameplayStatics::IsGamePaused(PC);
	if (!bWasPausedBefore)
	{
		UGameplayStatics::SetGamePaused(PC, true);
	}

	UE_LOG(LogQA, Display, TEXT("QA: MOCK-AD '%s' shown (paused=%s)"),
		*Placement.ToString(), bWasPausedBefore ? TEXT("already") : TEXT("by ad"));
}

void UMockAdService::HandleAdClosed()
{
	// Виджет уже снял себя с экрана (RemoveFromParent в HandleCloseClicked).
	APlayerController* PC = GetGameInstance() ? GetGameInstance()->GetFirstLocalPlayerController() : nullptr;
	if (PC && !bWasPausedBefore)
	{
		UGameplayStatics::SetGamePaused(PC, false);
	}

	ActiveWidget = nullptr;

	// Метка «игрок только что смотрел ролик с наградой» (как у боевой рекламы): по ней
	// полноэкранный показ без награды пропускается.
	if (!bActiveIsInterstitial)
	{
		LastRewardedShowUtc = FDateTime::UtcNow();
	}
	bActiveIsInterstitial = false;

	// Делегат зовём ПОСЛЕДНИМ: обработчик успеха может открыть следующий экран/новый показ.
	FSimpleDelegate Success = PendingSuccess;
	PendingSuccess.Unbind();
	Success.ExecuteIfBound();
}

void UMockAdService::ShowInterstitial(FName Placement, FSimpleDelegate OnClosed)
{
	// Тот же экран-заглушка, что у ролика; и отказ показа, и закрытие — это «закрылся».
	const bool bBusy = (ActiveWidget != nullptr);
	ShowRewarded(Placement, OnClosed, OnClosed);
	if (!bBusy && ActiveWidget)
	{
		bActiveIsInterstitial = true;
	}
}

void UMockAdService::ShowBanner(FName Placement)
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	if (BannerWidget)
	{
		BannerWidget->RemoveFromParent();
		BannerWidget = nullptr;
	}

	BannerWidget = CreateWidget<UMockBannerWidget>(PC, UMockBannerWidget::StaticClass());
	if (!BannerWidget)
	{
		BannerState.Reset();
		AdGameArea::Apply(GI, 0.0f);
		return;
	}

	// Заглушка «загружается» мгновенно, а её высота — предельная высота баннера из тех же
	// настроек, что у боевой рекламы на телефоне. Игра сжимается ровно на эту высоту
	// (решение Рината 04.10.2026: в редакторе должно быть видно то же, что на телефоне).
	BannerState.OnRequested(Placement);
	BannerState.OnLoaded(UContraryAdSettings::Get().BannerMaxHeightFraction);

	BannerWidget->Setup(Placement, BannerState.GetHeightFraction());
	// На ПОЛНОМ экране (AddToViewport, а не в области игры): полоса стоит в освободившемся
	// месте под сжатой игрой. Над окнами игры, но под «роликом» (300).
	BannerWidget->AddToViewport(/*ZOrder=*/250);
	AdGameArea::Apply(GI, BannerState.GetHeightFraction());

	UE_LOG(LogQA, Display, TEXT("QA: MOCK-AD banner '%s' shown (height %.3f of screen)"),
		*Placement.ToString(), BannerState.GetHeightFraction());
}

void UMockAdService::HideBanner(FName Placement)
{
	if (!BannerState.OnHidden(Placement))
	{
		return;
	}
	if (BannerWidget)
	{
		BannerWidget->RemoveFromParent();
		BannerWidget = nullptr;
	}
	// Игра сразу возвращается на весь экран.
	AdGameArea::Apply(GetGameInstance(), 0.0f);
	UE_LOG(LogQA, Display, TEXT("QA: MOCK-AD banner '%s' removed"), *Placement.ToString());
}

void UMockAdService::Deinitialize()
{
	// Масштаб интерфейса — общая настройка движка: при остановке игры в редакторе с
	// открытым экраном смерти или окном торговли обязаны вернуть её как была.
	if (BannerWidget)
	{
		BannerWidget->RemoveFromParent();
		BannerWidget = nullptr;
	}
	BannerState.Reset();
	AdGameArea::Apply(GetGameInstance(), 0.0f);
	Super::Deinitialize();
}

IAdService* AdService::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (!GI)
	{
		return nullptr;
	}

	// На Android работает боевая реализация поверх Yandex Mobile Ads. Её сабсистема
	// создаётся ТОЛЬКО там (UYandexAdService::ShouldCreateSubsystem), поэтому в редакторе
	// и на десктопе этот поиск ничего не находит и мы штатно падаем на прежнюю заглушку —
	// проверки в редакторе продолжают работать как раньше.
	if (UYandexAdService* Real = GI->GetSubsystem<UYandexAdService>())
	{
		return Real;
	}

	return GI->GetSubsystem<UMockAdService>();
}
