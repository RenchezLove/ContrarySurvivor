// Copyright ContrarySurvivor. Yandex Mobile Ads bridge module.

#pragma once

#include "CoreMinimal.h"

/**
 * Мост C++ <-> Java к Yandex Mobile Ads SDK (только вознаграждаемые ролики).
 *
 * Здесь НЕТ игровой логики: класс умеет ровно четыре вещи — поднять SDK, заказать ролик,
 * сказать «ролик лежит готовый», показать ролик. Что считать наградой, когда ставить игру
 * на паузу и какие события слать в аналитику — решает UYandexAdService в игровом модуле.
 *
 * Ролики опознаются по идентификатору рекламного места (ad unit id), а не по названию
 * игровой точки: если все три точки игры настроены на один и тот же идентификатор, в памяти
 * висит ОДИН предзагруженный ролик на всех, а не три.
 *
 * Вне Android все методы — пустышки, IsSupported() возвращает false. Так десктопная сборка
 * и редактор собираются и работают без единой заглушки в игровом коде.
 */

enum class EYandexAdEvent : uint8
{
	// SDK поднялся и готов принимать заказы на ролики (AdUnitId пустой).
	Initialized = 0,
	// Ролик загрузился и ждёт показа.
	Loaded = 1,
	// Загрузить не удалось (нет сети, нет заполнения и т.п.) — Detail содержит код и текст.
	LoadFailed = 2,
	// Ролик появился на экране.
	Shown = 3,
	// Техническая ошибка показа уже начатого ролика — Detail содержит текст ошибки.
	ShowFailed = 4,
	// Окно ролика закрылось. Была ли награда — смотреть по пришедшему ранее Rewarded.
	Dismissed = 5,
	// SDK засчитал просмотр и выдал награду.
	Rewarded = 6,
	// Показ заказали, а готового ролика нет. Признак ошибки в логике вызова, не ошибки сети.
	NoAdToShow = 7,
};

// События приходят УЖЕ В ИГРОВОМ ПОТОКЕ (мост сам перекладывает их с потока Android).
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnYandexAdEvent, const FString& /*AdUnitId*/, EYandexAdEvent /*Event*/, const FString& /*Detail*/);

class YANDEXADS_API FYandexAdsBridge
{
public:
	// Есть ли под этой платформой реальный SDK. Вне Android — false.
	static bool IsSupported();

	// Поднять SDK. Ответ придёт событием Initialized; повторный вызов безопасен.
	// bUserConsent — согласие пользователя на обработку данных в рекламных целях (GDPR).
	static void Initialize(bool bUserConsent, bool bEnableSdkLogging);

	// Заказать предзагрузку ролика. Повторный заказ, пока ролик грузится или уже лежит
	// готовым, ничего не делает.
	static void LoadRewarded(const FString& AdUnitId);

	// Показать предзагруженный ролик.
	static void ShowRewarded(const FString& AdUnitId);

	static FOnYandexAdEvent& OnAdEvent();
};
