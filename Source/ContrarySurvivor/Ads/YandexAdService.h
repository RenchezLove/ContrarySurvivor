// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ContrarySurvivor/Ads/AdService.h"
#include "YandexAdService.generated.h"

enum class EYandexAdEvent : uint8;

/**
 * Боевая реализация слоя рекламы поверх Yandex Mobile Ads SDK с медиацией Яндекса
 * (вторым источником через адаптер подключена VK Реклама). Формат один — вознаграждаемый
 * ролик. Живёт ТОЛЬКО на Android: на десктопе и в редакторе сабсистема не создаётся,
 * и AdService::Get() отдаёт прежнюю заглушку UMockAdService, чтобы проверки в редакторе
 * работали как раньше.
 *
 * Интерфейс IAdService не меняется, поэтому вся игровая логика (экран смерти, магазин,
 * ежедневная награда) остаётся нетронутой.
 *
 * Как это работает по шагам:
 *   1. При старте игры поднимаем SDK и сразу заказываем предзагрузку роликов.
 *   2. IsRewardedReady() отвечает честно: нет готового ролика — точки показа прячут кнопку.
 *   3. На время показа игра ставится на паузу и звук глушится; прежнее состояние
 *      запоминается и возвращается после закрытия ролика.
 *   4. Досмотрел до конца — OnSuccess. Закрыл сам раньше — OnFail.
 *   5. Техническая ошибка показа (сеть отвалилась, ошибка SDK) — OnSuccess, награду выдаём.
 *      Это осознанное требование издателя, а не недосмотр: игрок не должен страдать
 *      из-за проблем рекламной сети.
 *
 * Идентификаторы рекламных мест лежат в конфиге, а не в коде (секция
 * [/Script/ContrarySurvivor.YandexAdService] в Config/DefaultEngine.ini). По умолчанию
 * стоит демонстрационный идентификатор Яндекса — он отдаёт заглушечный ролик и позволяет
 * проверить всю логику до получения боевых ключей.
 */
UCLASS(Config = Engine)
class CONTRARYSURVIVOR_API UYandexAdService : public UGameInstanceSubsystem, public IAdService
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// --- IAdService ---
	virtual bool IsRewardedReady() const override;
	virtual void ShowRewarded(FName Placement, FSimpleDelegate OnSuccess, FSimpleDelegate OnFail) override;

	// Общий идентификатор рекламного места для всех трёх точек. Значение по умолчанию —
	// демонстрационное из документации Яндекса; боевое (вида R-M-XXXXXX-Y) подставляется
	// в конфиге, когда издатель его пришлёт.
	UPROPERTY(Config)
	FString RewardedAdUnitId = TEXT("demo-rewarded-yandex");

	// Отдельные идентификаторы под конкретные точки. Пустая строка означает «взять общий».
	UPROPERTY(Config)
	FString AdUnitIdDeathBackpack;

	UPROPERTY(Config)
	FString AdUnitIdShopSellBonus;

	UPROPERTY(Config)
	FString AdUnitIdDailyDouble;

	// Согласие пользователя на обработку данных в рекламных целях. Передаётся в SDK до
	// его инициализации.
	UPROPERTY(Config)
	bool bUserConsent = true;

	// Подробный журнал самого SDK в logcat. На боевой сборке выключать.
	UPROPERTY(Config)
	bool bEnableSdkLogging = false;

private:
	// Событие от моста (приходит в игровом потоке).
	void HandleAdEvent(const FString& AdUnitId, EYandexAdEvent Event, const FString& Detail);

	// Приложение вернулось на передний план — проверяем, не завис ли показ.
	void HandleApplicationForeground();

	// Идентификатор места для точки показа: свой, если задан, иначе общий.
	FString GetAdUnitIdForPlacement(FName Placement) const;

	// Все различные идентификаторы, которые вообще используются игрой.
	TArray<FString> GetConfiguredAdUnitIds() const;

	// Заказать предзагрузку (с защитой от повторного заказа — её держит Java-сторона).
	void RequestLoad(const FString& AdUnitId);

	// Отложенный перезаказ после неудачи, с растущей задержкой.
	void ScheduleRetry(const FString& AdUnitId);
	void CancelRetry(const FString& AdUnitId);

	// Завершить показ: вернуть паузу и звук, позвать нужный делегат ровно один раз.
	void FinishShow(bool bGrantReward, const TCHAR* Reason);

	void PauseGameForAd();
	void RestoreGameAfterAd();

	// Имя точки для аналитики: death_backpack -> backpack и так далее.
	static FString PlacementToAnalyticsPoint(FName Placement);

	// Какие идентификаторы прямо сейчас лежат готовыми к показу.
	TSet<FString> LoadedAdUnitIds;

	// Сколько раз подряд не удалось загрузить — задаёт задержку следующей попытки.
	TMap<FString, int32> LoadFailureCounts;

	TMap<FString, FDelegateHandle> RetryHandles;

	FDelegateHandle WatchdogHandle;

	FDelegateHandle AdEventHandle;
	FDelegateHandle ForegroundHandle;

	// --- состояние текущего показа ---
	bool bShowInProgress = false;
	bool bRewardGranted = false;
	FName ShowingPlacement;
	FString ShowingAdUnitId;
	FSimpleDelegate PendingSuccess;
	FSimpleDelegate PendingFail;

	// Пауза стояла ещё ДО ролика (например, меню) — тогда по закрытию её не снимаем.
	bool bWasPausedBefore = false;
	float VolumeBeforeAd = 1.0f;

	bool bSdkInitialized = false;
};
