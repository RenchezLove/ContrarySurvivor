// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Retention/DailyRewardLogic.h"

DailyReward::FComputeResult DailyReward::Compute(const FDateTime& Now, const FDateTime& LastRewardDate,
	int32 LastStreak, float BaseReward, float StepPerDay, float MaxReward)
{
	FComputeResult Result;

	const FDateTime Today = Now.GetDate();
	const FDateTime LastDay = LastRewardDate.GetDate();

	if (LastStreak <= 0 || LastRewardDate.GetTicks() == 0)
	{
		// Первый вход профиля — день 1.
		Result.bGrant = true;
		Result.NewStreak = 1;
	}
	else if (Today == LastDay)
	{
		// Повторный запуск в тот же календарный день — без награды, серия не меняется.
		Result.bGrant = false;
		Result.NewStreak = LastStreak;
	}
	else if (Today == LastDay + FTimespan::FromDays(1))
	{
		// Вход на следующий календарный день — серия растёт.
		Result.bGrant = true;
		Result.NewStreak = LastStreak + 1;
	}
	else if (Today < LastDay)
	{
		// Часы устройства перевели назад — не выдаём и серию не ломаем (анти-абьюз/сбой часов).
		Result.bGrant = false;
		Result.NewStreak = LastStreak;
	}
	else
	{
		// Пропущен минимум один календарный день — серия заново с дня 1.
		Result.bGrant = true;
		Result.NewStreak = 1;
	}

	if (Result.bGrant)
	{
		Result.Reward = FMath::Min(BaseReward + StepPerDay * (Result.NewStreak - 1), MaxReward);
	}
	return Result;
}
