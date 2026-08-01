// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Чистая логика условий показа rewarded-кнопок (Build 1.2, ТЗ издателя №1-№3, раздел 0) —
 * без UObject/мира, чтобы её гонял headless-автотест (Tests/AdsAutomationTests.cpp,
 * по образцу DailyRewardLogic).
 *
 * Правила ТЗ: глобальный запрет рекламы первые 15 минут СУММАРНОГО игрового времени с
 * установки (накопитель в сейве); суточные лимиты по КАЛЕНДАРНЫМ суткам локального
 * времени устройства; кулдаун точки магазина — от момента прошлого просмотра.
 */
namespace AdGating
{
	// 15 минут (ТЗ раздел 0 п.2). Единая константа всех трёх точек.
	inline constexpr double MinPlaytimeSeconds = 15.0 * 60.0;

	// Пройден ли глобальный порог игрового времени.
	CONTRARYSURVIVOR_API bool IsPlaytimeGatePassed(double TotalPlaySeconds,
		double MinSeconds = MinPlaytimeSeconds);

	// Сколько использований точки числится на СЕГОДНЯ: счётчик из сейва хранится парой
	// «календарная дата + число за эту дату»; дата сменилась (полночь) — счётчик равен 0.
	// CounterDate.Ticks == 0 означает «использований ещё не было».
	CONTRARYSURVIVOR_API int32 UsesToday(const FDateTime& Now, const FDateTime& CounterDate,
		int32 CounterUses);

	// Прошло ли CooldownSeconds с прошлого использования. LastUse.Ticks == 0 («ещё не
	// было») — пройден. Часы устройства перевели назад (LastUse в будущем) — НЕ пройден
	// до истечения кулдауна от «будущей» метки (анти-абьюз, как в DailyReward).
	CONTRARYSURVIVOR_API bool IsCooldownPassed(const FDateTime& Now, const FDateTime& LastUse,
		double CooldownSeconds);
}
