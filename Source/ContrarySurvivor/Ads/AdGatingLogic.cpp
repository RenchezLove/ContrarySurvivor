// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Ads/AdGatingLogic.h"

namespace AdGating
{
	bool IsPlaytimeGatePassed(double TotalPlaySeconds, double MinSeconds)
	{
		return TotalPlaySeconds >= MinSeconds;
	}

	bool IsAdGatePassed(double TotalPlaySeconds, bool bFirstQuestDone, double MinSeconds)
	{
		// «Что раньше»: сданный первый квест открывает рекламу до порога времени (РИ-29).
		return bFirstQuestDone || IsPlaytimeGatePassed(TotalPlaySeconds, MinSeconds);
	}

	int32 UsesToday(const FDateTime& Now, const FDateTime& CounterDate, int32 CounterUses)
	{
		if (CounterDate.GetTicks() == 0)
		{
			return 0; // использований ещё не было
		}
		// Сутки календарные, по локальному времени устройства (ТЗ №1 п.3): счётчик живёт
		// ровно в свою дату, любая другая дата (в т.ч. «вчера» после полуночи) даёт 0.
		return (CounterDate.GetDate() == Now.GetDate()) ? FMath::Max(0, CounterUses) : 0;
	}

	bool IsCooldownPassed(const FDateTime& Now, const FDateTime& LastUse, double CooldownSeconds)
	{
		if (LastUse.GetTicks() == 0)
		{
			return true; // ещё не использовалась
		}
		// Разница со знаком: LastUse в будущем (часы перевели назад) даёт отрицательные
		// секунды — кулдаун считается НЕ прошедшим, пока реальное время не догонит метку.
		const double SecondsSince = (Now - LastUse).GetTotalSeconds();
		return SecondsSince >= CooldownSeconds;
	}
}
