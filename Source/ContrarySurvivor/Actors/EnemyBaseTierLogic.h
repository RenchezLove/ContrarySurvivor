// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Misc/DateTime.h"

/**
 * Чистая логика ступеней базы противника (ТЗ издателя 22.08.2026 «возрождение базы и её
 * уровни», бэклог №31/№35). Без UObject — по образцу Retention/DailyRewardLogic: гоняется
 * headless-автотестом без мира и редактора; AMasterEnemyBase зовёт ровно эти функции,
 * а не держит копию правил.
 *
 * Правила владельца (дословно из ТЗ):
 *  - ступеней пять; каждая ПОЛНАЯ зачистка поднимает на одну;
 *  - «после зачистки пятой ступени база возрождается на третьей» — цикл 3→4→5→3;
 *  - паузы до возрождения — ПО ЗАЧИЩЕННОЙ ступени (15/30/45/55/60 мин по умолчанию);
 *  - время — РЕАЛЬНЫЕ часы (UTC: смена часового пояса устройства не дарит халявы);
 *  - прибавка здоровья по ступени (+0/12/19/22/27) — числа приходят ОТ БАЗЫ.
 */
namespace EnemyBaseTierLogic
{
	// Границы ступеней (ТЗ: «ступеней пять»).
	constexpr int32 MinTier = 1;
	constexpr int32 MaxTier = 5;

	// Ступень, на которую база возрождается после зачистки пятой (цикл 3→4→5→3).
	constexpr int32 TierAfterMax = 3;

	// Ступень всегда в границах 1..5 (защита от кривой настройки экземпляра).
	inline int32 ClampTier(int32 Tier)
	{
		return FMath::Clamp(Tier, MinTier, MaxTier);
	}

	// Следующая ступень после ПОЛНОЙ зачистки ступени ClearedTier: +1, а после пятой — третья.
	inline int32 NextTierAfterClear(int32 ClearedTier)
	{
		const int32 Cleared = ClampTier(ClearedTier);
		return (Cleared >= MaxTier) ? TierAfterMax : Cleared + 1;
	}

	// Значение «по ступени» из настроечного массива базы: индекс = ступень − 1; массив короче
	// пяти — берётся последний элемент (заполнили три значения — старшие ступени живут по
	// последнему), пустой массив — Fallback.
	inline float PerTierValue(int32 Tier, const TArray<float>& PerTier, float Fallback)
	{
		if (PerTier.Num() == 0)
		{
			return Fallback;
		}
		const int32 Index = FMath::Clamp(ClampTier(Tier) - 1, 0, PerTier.Num() - 1);
		return PerTier[Index];
	}

	// Пауза до возрождения (минуты) по ЗАЧИЩЕННОЙ ступени.
	inline float PauseMinutesForClearedTier(int32 ClearedTier, const TArray<float>& PauseMinutesPerTier)
	{
		return PerTierValue(ClearedTier, PauseMinutesPerTier, /*Fallback=*/60.0f);
	}

	// Прибавка здоровья врагам ступени Tier.
	inline float HealthBonusForTier(int32 Tier, const TArray<float>& BonusPerTier)
	{
		return FMath::Max(0.0f, PerTierValue(Tier, BonusPerTier, /*Fallback=*/0.0f));
	}

	// Истекла ли пауза возрождения к моменту NowUtc. Время зачистки нулевое (зачисток не
	// было) — пауза считается истёкшей: свежая база живёт по обычной активации.
	inline bool IsRespawnPauseElapsed(const FDateTime& ClearTimeUtc, const FDateTime& NowUtc,
		float PauseMinutes)
	{
		if (ClearTimeUtc.GetTicks() == 0)
		{
			return true;
		}
		return (NowUtc - ClearTimeUtc) >= FTimespan::FromMinutes(FMath::Max(0.0f, PauseMinutes));
	}
}
