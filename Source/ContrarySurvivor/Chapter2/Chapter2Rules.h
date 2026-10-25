// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Глава 2 «Вера», задание «Образцы из аномалий»: идентификаторы и чистые правила (без UObject),
 * чтобы каждое правило задания держала автопроверка без мира.
 */
namespace Chapter2Ids
{
	// Метка Веры (ATalkerNPC::NpcTag): ей сдаётся задание, по ней строка «Выдал» в смартфоне.
	inline const FName VeraTag(TEXT("ScientistVera"));

	// Задание «Образцы из аномалий».
	inline const FName SamplesQuest(TEXT("AnomalySamples"));

	// Событие цели «подобран образец» (счётчик задания, UQuestComponent::NotifyObjectiveEvent).
	inline const FName SampleObjectiveTag(TEXT("AnomalySample"));

	// Метка актора низины: на неё ведёт метка задания на экране и на карте смартфона.
	inline const FName LowlandMarkerTag(TEXT("AnomalyLowland"));

	// Метка собеседника-наёмника (окно разговора перед боем).
	inline const FName MercenaryTag(TEXT("Chapter2Mercenary"));
}

namespace Chapter2Rules
{
	/** Сколько здоровья в секунду отнимает воздух вокруг героя (0 — дышать можно). */
	inline float ZoneDamagePerSecond(bool bInZone, bool bMaskOn, float AirSeconds, float NoMaskDps, float NoAirDps)
	{
		if (!bInZone)
		{
			return 0.0f;
		}
		if (!bMaskOn)
		{
			return FMath::Max(0.0f, NoMaskDps);
		}
		return AirSeconds > 0.0f ? 0.0f : FMath::Max(0.0f, NoAirDps);
	}

	/** Воздух после Dt секунд: тратится, только пока аппарат надет. */
	inline float DrainAir(float AirSeconds, bool bMaskOn, float Dt)
	{
		return bMaskOn ? FMath::Max(0.0f, AirSeconds - FMath::Max(0.0f, Dt)) : AirSeconds;
	}

	/** Сила сигнала детектора 0..1: 1 вплотную к образцу, 0 на краю дальности и дальше. */
	inline float SignalStrength(float DistanceToSample, float Range)
	{
		if (DistanceToSample < 0.0f || Range <= 0.0f || DistanceToSample >= Range)
		{
			return 0.0f;
		}
		return FMath::Clamp(1.0f - DistanceToSample / Range, 0.0f, 1.0f);
	}

	/**
	 * Пауза между писками при такой силе сигнала: чем ближе образец, тем чаще.
	 * Меньше нуля — сигнала нет, детектор молчит.
	 */
	inline float BeepInterval(float Strength, float NearInterval, float FarInterval)
	{
		if (Strength <= 0.0f)
		{
			return -1.0f;
		}
		return FMath::Lerp(FMath::Max(0.05f, FarInterval), FMath::Max(0.05f, NearInterval), FMath::Clamp(Strength, 0.0f, 1.0f));
	}

	/**
	 * Пора ли выпускать наёмника (поправка Рината 25.10: «по дороге назад после квеста
	 * "образцы аномалий" на героя нападает наёмник куратора»): собраны ВСЕ образцы, наёмник
	 * ещё не побеждён, герой уже вышел из низины.
	 */
	inline bool ShouldSpawnMercenary(int32 SamplesDone, int32 SamplesTotal, bool bMercenaryDefeated, bool bHeroInZone)
	{
		return SamplesTotal > 0 && SamplesDone >= SamplesTotal && !bMercenaryDefeated && !bHeroInZone;
	}
}
