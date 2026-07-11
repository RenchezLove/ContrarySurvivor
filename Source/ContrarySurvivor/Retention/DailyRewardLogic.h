// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Чистая логика ежедневной награды (Этап F2, ADR-044 п.4) — без UObject/мира, чтобы её
 * гонял headless-автотест (Tests/RetentionAutomationTests.cpp).
 *
 * Правила (Ринат): день 1 = базовая сумма, каждый следующий день серии +шаг, до потолка;
 * пропуск календарного дня сбрасывает серию на день 1; повторный вход в тот же день —
 * без награды. Сравнение — по КАЛЕНДАРНЫМ датам локального времени устройства.
 */
namespace DailyReward
{
	struct FComputeResult
	{
		// Выдавать ли награду (календарная дата сменилась либо это первый вход профиля).
		bool bGrant = false;

		// Новая серия дней подряд (>=1 при bGrant; без выдачи — прежняя серия).
		int32 NewStreak = 0;

		// Сумма монет к выдаче (0, если bGrant == false).
		float Reward = 0.0f;
	};

	// Now — текущий момент; LastRewardDate — дата последнего засчитанного входа из сейва
	// (Ticks == 0 вместе с LastStreak <= 0 означает «входов ещё не было»).
	CONTRARYSURVIVOR_API FComputeResult Compute(const FDateTime& Now, const FDateTime& LastRewardDate,
		int32 LastStreak, float BaseReward, float StepPerDay, float MaxReward);
}
