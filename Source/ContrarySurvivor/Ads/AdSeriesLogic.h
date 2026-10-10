// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Чистая логика правила «не больше N просмотров подряд, потом перерыв» (GDD §12: реклама как
 * «эксперименты учёных» — «не более 3 экспериментов подряд с перерывом 3–5 минут»). Без мира и
 * объектов движка — гоняется автотестом (по образцу Ads/AdGatingLogic.h).
 *
 * «Подряд» — значит без перерыва между просмотрами: если с прошлого просмотра прошло не меньше
 * перерыва, серия начинается заново.
 */
struct FAdSeriesState
{
	// Сколько просмотров в текущей серии.
	int32 ViewsInSeries = 0;

	// Когда закончился последний просмотр (UTC). Ticks == 0 — просмотров ещё не было.
	FDateTime LastViewUtc;
};

namespace AdSeries
{
	// Сколько секунд перерыва осталось. Ноль — смотреть можно. Часы устройства перевели назад
	// (последний просмотр «в будущем») — перерыв отсчитывается заново целиком.
	inline double BreakSecondsLeft(const FAdSeriesState& State, const FDateTime& NowUtc,
		int32 MaxViewsInRow, double BreakSeconds)
	{
		if (MaxViewsInRow <= 0 || State.ViewsInSeries < MaxViewsInRow || State.LastViewUtc.GetTicks() == 0)
		{
			return 0.0;
		}
		const double Elapsed = (NowUtc - State.LastViewUtc).GetTotalSeconds();
		if (Elapsed < 0.0)
		{
			return FMath::Max(0.0, BreakSeconds);
		}
		return FMath::Max(0.0, BreakSeconds - Elapsed);
	}

	inline bool CanWatch(const FAdSeriesState& State, const FDateTime& NowUtc,
		int32 MaxViewsInRow, double BreakSeconds)
	{
		return BreakSecondsLeft(State, NowUtc, MaxViewsInRow, BreakSeconds) <= 0.0;
	}

	// Засчитать досмотренный просмотр.
	inline void RegisterView(FAdSeriesState& State, const FDateTime& NowUtc, double BreakSeconds)
	{
		if (State.LastViewUtc.GetTicks() != 0
			&& (NowUtc - State.LastViewUtc).GetTotalSeconds() >= BreakSeconds)
		{
			State.ViewsInSeries = 0; // перерыв выдержан — серия начинается заново
		}
		++State.ViewsInSeries;
		State.LastViewUtc = NowUtc;
	}
}
