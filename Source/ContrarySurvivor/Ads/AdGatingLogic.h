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
	// ADR-063 п.3 (РИ-29, было 360 по В1): порог больше НЕ жёсткая константа трёх точек —
	// живое значение настраивается EditAnywhere на игроке
	// (APlayerCharacter::AdMinPlaytimeSeconds), все три точки показа передают его сюда.
	// Эта константа осталась дефолтом поля и дефолт-аргументом чистых функций
	// (headless-тесты гоняют их без UObject).
	inline constexpr double MinPlaytimeSeconds = 300.0;

	// Пройден ли глобальный порог игрового времени.
	CONTRARYSURVIVOR_API bool IsPlaytimeGatePassed(double TotalPlaySeconds,
		double MinSeconds = MinPlaytimeSeconds);

	// ADR-063 п.3 (РИ-29, дословно): «300 секунд игры ИЛИ выполнен первый квест, что
	// раньше, на всех трёх точках, включая экран смерти». Единая проверка порога для всех
	// трёх точек показа (магазин / ежедневная награда / «Спасти рюкзак» экрана смерти);
	// bFirstQuestDone — сдан ли первый квест (APlayerCharacter::HasTurnedInFirstQuest).
	CONTRARYSURVIVOR_API bool IsAdGatePassed(double TotalPlaySeconds, bool bFirstQuestDone,
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
