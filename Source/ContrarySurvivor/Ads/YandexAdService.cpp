// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Ads/YandexAdService.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "YandexAdsBridge.h"

namespace YandexAdServiceLocal
{
	// Задержки повторного заказа ролика после неудачи: первая, вторая, все последующие.
	// Без них одна пропавшая сеть выключала бы рекламу до конца сессии.
	static constexpr float FirstRetrySeconds = 5.0f;
	static constexpr float SecondRetrySeconds = 15.0f;
	static constexpr float LaterRetrySeconds = 60.0f;

	// Сколько ждать событие о закрытии ролика после возвращения игры на передний план.
	// Страховка от зависшей паузы, если окно рекламы умерло, не позвав ни один обработчик.
	static constexpr float WatchdogSeconds = 4.0f;
}

bool UYandexAdService::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}

	// Реальный SDK есть только под Android. В редакторе и на десктопе сабсистемы нет,
	// поэтому AdService::Get() возвращает прежнюю заглушку и проверки в редакторе живут.
	return FYandexAdsBridge::IsSupported();
}

void UYandexAdService::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	AdEventHandle = FYandexAdsBridge::OnAdEvent().AddUObject(this, &UYandexAdService::HandleAdEvent);
	ForegroundHandle = FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(
		this, &UYandexAdService::HandleApplicationForeground);

	// Б6: SDK здесь НЕ поднимаем. Согласие передаётся в SDK ровно один раз, при его
	// инициализации, поэтому ждём ответа игрока — его пришлёт UDataConsentSubsystem
	// вызовом ApplyUserConsent (сразу при старте, если игрок отвечал в прошлые запуски,
	// либо после экрана согласия при самом первом запуске).
	UE_LOG(LogQA, Display, TEXT("QA: YANDEX-AD waiting for user consent before SDK init (sdk log=%s)"),
		bEnableSdkLogging ? TEXT("on") : TEXT("off"));
}

void UYandexAdService::ApplyUserConsent(bool bGranted)
{
	if (bInitializeRequested)
	{
		// Игрок передумал уже после запуска. У Яндекса согласие меняется только новой
		// инициализацией SDK, а второй раз её звать нельзя — новое значение вступит в силу
		// со следующего запуска игры. Сохранённое решение к тому моменту уже лежит на диске.
		UE_LOG(LogQA, Display,
			TEXT("QA: YANDEX-AD consent changed to '%s' - applies on next game launch (SDK already initialized)"),
			bGranted ? TEXT("yes") : TEXT("no"));
		return;
	}

	bInitializeRequested = true;
	UE_LOG(LogQA, Display, TEXT("QA: YANDEX-AD init requested (consent=%s, sdk log=%s)"),
		bGranted ? TEXT("yes") : TEXT("no"), bEnableSdkLogging ? TEXT("on") : TEXT("off"));

	FYandexAdsBridge::Initialize(bGranted, bEnableSdkLogging);
}

void UYandexAdService::Deinitialize()
{
	if (AdEventHandle.IsValid())
	{
		FYandexAdsBridge::OnAdEvent().Remove(AdEventHandle);
		AdEventHandle.Reset();
	}
	if (ForegroundHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Remove(ForegroundHandle);
		ForegroundHandle.Reset();
	}

	for (const TPair<FString, FTSTicker::FDelegateHandle>& Pair : RetryHandles)
	{
		FTSTicker::RemoveTicker(Pair.Value);
	}
	RetryHandles.Empty();

	if (WatchdogHandle.IsValid())
	{
		FTSTicker::RemoveTicker(WatchdogHandle);
		WatchdogHandle.Reset();
	}

	Super::Deinitialize();
}

bool UYandexAdService::IsRewardedReady() const
{
	if (!bSdkInitialized || bShowInProgress)
	{
		return false;
	}

	const TArray<FString> Units = GetConfiguredAdUnitIds();
	if (Units.Num() == 0)
	{
		return false;
	}

	// Интерфейс спрашивает про готовность вообще, без указания точки, поэтому отвечаем
	// строго: готовы все настроенные рекламные места. При обычной настройке (одно место
	// на все три точки) это ровно «ролик лежит и ждёт».
	for (const FString& Unit : Units)
	{
		if (!LoadedAdUnitIds.Contains(Unit))
		{
			return false;
		}
	}
	return true;
}

void UYandexAdService::ShowRewarded(FName Placement, FSimpleDelegate OnSuccess, FSimpleDelegate OnFail)
{
	// Один ролик за раз: повторный вызов до закрытия — отказ показа, награды нет.
	if (bShowInProgress)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: YANDEX-AD '%s' rejected - another ad is already showing"),
			*Placement.ToString());
		OnFail.ExecuteIfBound();
		return;
	}

	const FString AdUnitId = GetAdUnitIdForPlacement(Placement);
	if (AdUnitId.IsEmpty())
	{
		UE_LOG(LogQA, Warning, TEXT("QA: YANDEX-AD '%s' rejected - no ad unit id in config"),
			*Placement.ToString());
		OnFail.ExecuteIfBound();
		return;
	}

	if (!LoadedAdUnitIds.Contains(AdUnitId))
	{
		// Сюда попадать не должны: кнопку показывают только при IsRewardedReady() == true.
		UE_LOG(LogQA, Warning, TEXT("QA: YANDEX-AD '%s' rejected - no preloaded ad for unit '%s'"),
			*Placement.ToString(), *AdUnitId);
		RequestLoad(AdUnitId);
		OnFail.ExecuteIfBound();
		return;
	}

	bShowInProgress = true;
	bRewardGranted = false;
	ShowingPlacement = Placement;
	ShowingAdUnitId = AdUnitId;
	PendingSuccess = OnSuccess;
	PendingFail = OnFail;

	// Ролик одноразовый: с этого момента он больше не «готовый».
	LoadedAdUnitIds.Remove(AdUnitId);

	PauseGameForAd();

	UE_LOG(LogQA, Display, TEXT("QA: YANDEX-AD '%s' show requested (unit '%s')"),
		*Placement.ToString(), *AdUnitId);

	FYandexAdsBridge::ShowRewarded(AdUnitId);
}

void UYandexAdService::HandleAdEvent(const FString& AdUnitId, EYandexAdEvent Event, const FString& Detail)
{
	switch (Event)
	{
	case EYandexAdEvent::Initialized:
	{
		bSdkInitialized = true;
		UE_LOG(LogQA, Display, TEXT("QA: YANDEX-AD sdk initialized (version %s)"), *Detail);

		// Предзагрузка заранее (требование издателя): ролик заказываем сразу, чтобы к
		// моменту смерти или открытия магазина он уже лежал готовым.
		for (const FString& Unit : GetConfiguredAdUnitIds())
		{
			RequestLoad(Unit);
		}
		break;
	}

	case EYandexAdEvent::Loaded:
	{
		LoadedAdUnitIds.Add(AdUnitId);
		LoadFailureCounts.Remove(AdUnitId);
		CancelRetry(AdUnitId);
		UE_LOG(LogQA, Display, TEXT("QA: YANDEX-AD loaded (unit '%s')"), *AdUnitId);
		break;
	}

	case EYandexAdEvent::LoadFailed:
	{
		LoadedAdUnitIds.Remove(AdUnitId);
		int32& Failures = LoadFailureCounts.FindOrAdd(AdUnitId);
		++Failures;
		UE_LOG(LogQA, Warning, TEXT("QA: YANDEX-AD load failed (unit '%s', attempt %d) - %s"),
			*AdUnitId, Failures, *Detail);
		ScheduleRetry(AdUnitId);
		break;
	}

	case EYandexAdEvent::Shown:
	{
		UE_LOG(LogQA, Display, TEXT("QA: YANDEX-AD shown (unit '%s')"), *AdUnitId);
		break;
	}

	case EYandexAdEvent::Rewarded:
	{
		// Награду фиксируем, но делегат зовём только после закрытия окна ролика.
		bRewardGranted = true;
		UE_LOG(LogQA, Display, TEXT("QA: YANDEX-AD rewarded (unit '%s') - %s"), *AdUnitId, *Detail);
		break;
	}

	case EYandexAdEvent::Dismissed:
	{
		// Досмотрел до конца — награда есть. Закрыл раньше — награды нет.
		FinishShow(bRewardGranted, bRewardGranted ? TEXT("completed") : TEXT("dismissed"));
		break;
	}

	case EYandexAdEvent::ShowFailed:
	{
		// Осознанное требование издателя: техническая ошибка сети или SDK — награду выдаём.
		UE_LOG(LogQA, Warning, TEXT("QA: YANDEX-AD show failed (unit '%s') - %s, reward granted anyway"),
			*AdUnitId, *Detail);
		if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
		{
			Analytics->RecordAdStage(PlacementToAnalyticsPoint(ShowingPlacement), TEXT("failed"));
		}
		FinishShow(/*bGrantReward=*/true, TEXT("show failed"));
		break;
	}

	case EYandexAdEvent::NoAdToShow:
	{
		// Показ заказали, а ролика нет. Это ошибка нашей логики, а не сбой сети,
		// поэтому награду НЕ выдаём.
		UE_LOG(LogQA, Warning, TEXT("QA: YANDEX-AD nothing to show (unit '%s') - %s"), *AdUnitId, *Detail);
		FinishShow(/*bGrantReward=*/false, TEXT("no ad"));
		break;
	}

	default:
		break;
	}
}

void UYandexAdService::HandleApplicationForeground()
{
	// Игра вернулась на передний план. Если показ всё ещё числится идущим, ждём событие
	// закрытия совсем недолго — иначе снимаем паузу сами, чтобы игра не осталась висеть.
	if (!bShowInProgress || WatchdogHandle.IsValid())
	{
		return;
	}

	TWeakObjectPtr<UYandexAdService> WeakThis(this);
	WatchdogHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("YandexAdWatchdog"),
		YandexAdServiceLocal::WatchdogSeconds,
		[WeakThis](float) -> bool
		{
			if (UYandexAdService* Self = WeakThis.Get())
			{
				Self->WatchdogHandle.Reset();
				if (Self->bShowInProgress)
				{
					UE_LOG(LogQA, Warning, TEXT("QA: YANDEX-AD no close event after returning to game - releasing pause"));
					if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(Self))
					{
						Analytics->RecordAdStage(PlacementToAnalyticsPoint(Self->ShowingPlacement), TEXT("failed"));
					}
					// Считаем это технической ошибкой — по правилу издателя награда выдаётся.
					Self->FinishShow(/*bGrantReward=*/true, TEXT("watchdog"));
				}
			}
			return false;
		});
}

void UYandexAdService::FinishShow(bool bGrantReward, const TCHAR* Reason)
{
	if (!bShowInProgress)
	{
		return;
	}
	bShowInProgress = false;

	if (WatchdogHandle.IsValid())
	{
		FTSTicker::RemoveTicker(WatchdogHandle);
		WatchdogHandle.Reset();
	}

	RestoreGameAfterAd();

	const FString FinishedUnit = ShowingAdUnitId;
	const FName FinishedPlacement = ShowingPlacement;
	ShowingAdUnitId.Reset();
	ShowingPlacement = NAME_None;
	bRewardGranted = false;

	UE_LOG(LogQA, Display, TEXT("QA: YANDEX-AD '%s' finished (%s), reward=%s"),
		*FinishedPlacement.ToString(), Reason, bGrantReward ? TEXT("yes") : TEXT("no"));

	FSimpleDelegate Success = PendingSuccess;
	FSimpleDelegate Fail = PendingFail;
	PendingSuccess.Unbind();
	PendingFail.Unbind();

	// Следующий ролик заказываем заранее, ещё до обработчика результата: обработчик
	// успеха может открыть новый экран и тут же спросить готовность.
	if (!FinishedUnit.IsEmpty())
	{
		RequestLoad(FinishedUnit);
	}

	// Делегат зовём последним — он может привести к новому показу.
	if (bGrantReward)
	{
		Success.ExecuteIfBound();
	}
	else
	{
		Fail.ExecuteIfBound();
	}
}

void UYandexAdService::PauseGameForAd()
{
	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;

	bWasPausedBefore = PC ? UGameplayStatics::IsGamePaused(PC) : false;
	if (PC && !bWasPausedBefore)
	{
		UGameplayStatics::SetGamePaused(PC, true);
	}

	// Звук глушим общим множителем громкости приложения и возвращаем прежнее значение
	// после ролика (движок сам приглушает звук при уходе в фон, но полагаться только на
	// это нельзя: окно рекламы не всегда уводит игру в фон).
	VolumeBeforeAd = FApp::GetVolumeMultiplier();
	FApp::SetVolumeMultiplier(0.0f);
}

void UYandexAdService::RestoreGameAfterAd()
{
	FApp::SetVolumeMultiplier(VolumeBeforeAd);

	UGameInstance* GI = GetGameInstance();
	APlayerController* PC = GI ? GI->GetFirstLocalPlayerController() : nullptr;
	if (PC && !bWasPausedBefore)
	{
		UGameplayStatics::SetGamePaused(PC, false);
	}
}

FString UYandexAdService::GetAdUnitIdForPlacement(FName Placement) const
{
	if (Placement == AdPlacements::DeathBackpack && !AdUnitIdDeathBackpack.IsEmpty())
	{
		return AdUnitIdDeathBackpack;
	}
	if (Placement == AdPlacements::ShopSellBonus && !AdUnitIdShopSellBonus.IsEmpty())
	{
		return AdUnitIdShopSellBonus;
	}
	if (Placement == AdPlacements::DailyDouble && !AdUnitIdDailyDouble.IsEmpty())
	{
		return AdUnitIdDailyDouble;
	}
	if (Placement == AdPlacements::SupportAuthor && !AdUnitIdSupportAuthor.IsEmpty())
	{
		return AdUnitIdSupportAuthor;
	}
	return RewardedAdUnitId;
}

TArray<FString> UYandexAdService::GetConfiguredAdUnitIds() const
{
	TArray<FString> Units;
	const FName AllPlacements[] = { AdPlacements::DeathBackpack, AdPlacements::ShopSellBonus,
		AdPlacements::DailyDouble, AdPlacements::SupportAuthor };
	for (const FName& Placement : AllPlacements)
	{
		const FString Unit = GetAdUnitIdForPlacement(Placement);
		if (!Unit.IsEmpty())
		{
			Units.AddUnique(Unit);
		}
	}
	return Units;
}

void UYandexAdService::RequestLoad(const FString& AdUnitId)
{
	if (AdUnitId.IsEmpty() || LoadedAdUnitIds.Contains(AdUnitId))
	{
		return;
	}
	FYandexAdsBridge::LoadRewarded(AdUnitId);
}

void UYandexAdService::ScheduleRetry(const FString& AdUnitId)
{
	CancelRetry(AdUnitId);

	const int32 Failures = LoadFailureCounts.FindRef(AdUnitId);
	float Delay = YandexAdServiceLocal::LaterRetrySeconds;
	if (Failures <= 1)
	{
		Delay = YandexAdServiceLocal::FirstRetrySeconds;
	}
	else if (Failures == 2)
	{
		Delay = YandexAdServiceLocal::SecondRetrySeconds;
	}

	TWeakObjectPtr<UYandexAdService> WeakThis(this);
	const FString Unit = AdUnitId;
	const FTSTicker::FDelegateHandle Handle = FTSTicker::GetCoreTicker().AddTicker(TEXT("YandexAdRetry"), Delay,
		[WeakThis, Unit](float) -> bool
		{
			if (UYandexAdService* Self = WeakThis.Get())
			{
				Self->RetryHandles.Remove(Unit);
				Self->RequestLoad(Unit);
			}
			return false;
		});
	RetryHandles.Add(AdUnitId, Handle);
}

void UYandexAdService::CancelRetry(const FString& AdUnitId)
{
	if (const FTSTicker::FDelegateHandle* Handle = RetryHandles.Find(AdUnitId))
	{
		FTSTicker::RemoveTicker(*Handle);
		RetryHandles.Remove(AdUnitId);
	}
}

FString UYandexAdService::PlacementToAnalyticsPoint(FName Placement)
{
	// Имена точек берутся из таблицы соответствия в шапке AnalyticsSubsystem.h.
	if (Placement == AdPlacements::DeathBackpack)
	{
		return TEXT("backpack");
	}
	if (Placement == AdPlacements::ShopSellBonus)
	{
		return TEXT("shop");
	}
	if (Placement == AdPlacements::DailyDouble)
	{
		return TEXT("daily");
	}
	if (Placement == AdPlacements::SupportAuthor)
	{
		return TEXT("support");
	}
	return TEXT("unknown");
}
