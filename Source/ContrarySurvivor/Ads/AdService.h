// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AdService.generated.h"

/**
 * Слой абстракции рекламы (Build 1.2, указание Рината): вся игровая логика ходит ТОЛЬКО
 * через IAdService и не знает, какая рекламная сеть под ним. Сеть ещё не выбрана — на эту
 * ночь единственная реализация UMockAdService (заглушка «здесь будет ролик»); когда сеть
 * выберут, реальный SDK встанет за тем же интерфейсом, точки вызова не меняются.
 *
 * Нативный C++-интерфейс (по образцу IShopVendor): методы — плейн-virtual, НЕ UFUNCTION,
 * вызовы только из C++ (виджеты/компоненты), делегаты BlueprintNativeEvent не переварит.
 */

// Placement-ключи трёх точек rewarded-рекламы Build 1 (ТЗ издателя №1-№3).
// Строки фиксированы заданием — по ним же считается аналитика.
namespace AdPlacements
{
	// Экран смерти: «Спасти рюкзак» (ТЗ №1).
	inline const FName DeathBackpack(TEXT("death_backpack"));

	// Магазин: «Продать дороже» +50% (ТЗ №2).
	inline const FName ShopSellBonus(TEXT("shop_sell_bonus"));

	// Ежедневная награда: «Забрать вдвое больше» (ТЗ №3).
	inline const FName DailyDouble(TEXT("daily_double"));

	// Окно «Поддержать автора» (задание издателя, решение Рината 11.08.2026) — ЧЕТВЁРТАЯ точка.
	// Отличается от трёх предыдущих двумя вещами, и обе принципиальны:
	//   • игрок приходит сюда САМ, поэтому порог по игровому времени (правило РИ-29,
	//     AdGating::IsAdGatePassed) на неё НЕ распространяется и не проверяется вовсе;
	//   • за просмотр игроку НИЧЕГО не выдаётся — иначе это обычная ежедневка и замер сломан.
	inline const FName SupportAuthor(TEXT("support_author"));
}

UINTERFACE(MinimalAPI)
class UAdService : public UInterface
{
	GENERATED_BODY()
};

class IAdService
{
	GENERATED_BODY()

public:
	// Готов ли rewarded-ролик к показу. Кнопки рекламы обязаны проверять ЭТО условие
	// (ТЗ раздел 0 п.5: ролик не готов — кнопка не показывается вообще). У заглушки
	// всегда true, но точки вызова держат проверку честно — под будущую реальную сеть.
	virtual bool IsRewardedReady() const = 0;

	// Показать rewarded-ролик. OnSuccess — ролик досмотрен, награду выдавать;
	// OnFail — закрыт досрочно/ошибка показа, награду не выдавать (ТЗ раздел 0 п.6;
	// техническая ошибка СЕТИ на середине ролика у реального SDK пойдёт в OnSuccess —
	// это решение реализации, не интерфейса). Делегаты зовутся ровно один раз.
	virtual void ShowRewarded(FName Placement, FSimpleDelegate OnSuccess, FSimpleDelegate OnFail) = 0;
};

namespace AdService
{
	// Активная реализация слоя рекламы (сейчас — UMockAdService на GameInstance).
	// null вне игры/без GameInstance — точки вызова обязаны переживать null.
	CONTRARYSURVIVOR_API IAdService* Get(const UObject* WorldContextObject);
}
