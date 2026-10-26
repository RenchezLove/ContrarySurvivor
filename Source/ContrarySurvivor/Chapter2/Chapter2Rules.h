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

	// Задание-цель «Поговорить с Верой» [230] (правка Рината 25.10 вечер): появляется, когда
	// ноутбук сдан учёному, и уходит из журнала, когда взято задание «Образцы из аномалий».
	inline const FName TalkToVeraQuest(TEXT("TalkToVera"));

	// Служебные ключи снаряжения учёных (AMasterInventoryItem::ItemName): баллон и детектор.
	inline const TCHAR* const AirTankKey = TEXT("ScientistAirTank");
	inline const TCHAR* const DetectorKey = TEXT("ScientistDetector");
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

	/**
	 * Детектор-локатор (правка Рината 25.10 вечер): виден, пока задание взято, образцы собраны
	 * не все и герой в низине или не дальше порога от её края. ZoneDistance — расстояние до
	 * края низины (0 — внутри, меньше нуля — низины на уровне нет).
	 */
	inline bool ShouldShowLocator(bool bQuestActive, int32 SamplesLeft, bool bInZone, float ZoneDistance, float ShowDistance)
	{
		if (!bQuestActive || SamplesLeft <= 0)
		{
			return false;
		}
		return bInZone || (ZoneDistance >= 0.0f && ZoneDistance <= FMath::Max(0.0f, ShowDistance));
	}

	/**
	 * Куда на экране локатора смотрит образец: направление в точках экрана (X вправо, Y вниз),
	 * длина 1. Камера смотрит сверху, верх экрана — куда смотрит камера по рысканию: так
	 * направление на локаторе совпадает с направлением на образец на экране игры при любом
	 * повороте камеры. WorldDelta — от героя до образца в плоскости XY. Нулевой — (0, -1).
	 */
	inline FVector2D LocatorScreenDir(const FVector2D& WorldDelta, float CameraYawDeg)
	{
		const double Yaw = FMath::DegreesToRadians(static_cast<double>(CameraYawDeg));
		const FVector2D Forward(FMath::Cos(Yaw), FMath::Sin(Yaw));
		const FVector2D Right(-FMath::Sin(Yaw), FMath::Cos(Yaw));
		FVector2D Screen(FVector2D::DotProduct(WorldDelta, Right), -FVector2D::DotProduct(WorldDelta, Forward));
		return Screen.Normalize() ? Screen : FVector2D(0.0, -1.0);
	}

	/** Появилась ли подсказка-стрелка: столько секунд локатор на экране без подбора образца. */
	inline bool ShouldShowHintArrow(float SecondsWithoutPickup, float ArrowDelay)
	{
		return SecondsWithoutPickup >= FMath::Max(0.0f, ArrowDelay);
	}

	/** Где стоит фотография из ноутбука и докуда должно опуститься окно разговора. */
	struct FPhotoPlacement
	{
		FBox2D Photo = FBox2D(ForceInit); // место фотографии (пустое — ставить некуда)
		double DialogTop = 0.0;           // верх окна разговора не выше этого (0 — окно не трогать)
	};

	/**
	 * Фотография из ноутбука (правка Рината 26.10.2026: «Фото теперь вообще маленькое сбоку
	 * появляется»; решение лида): пока она показана, она крупно стоит по центру В ВЕРХНЕЙ
	 * свободной части экрана над окном разговора. Высота — всё место над окном минус отступы
	 * Margin, но не меньше MinHeightFraction высоты экрана; если места над окном меньше, окно
	 * разговора на это время опускается (DialogTop — новый верх окна). Пропорции фотографии
	 * сохраняются (Aspect = ширина / высота); шире экрана она не бывает.
	 * Screen — размер экрана, DialogBox — окно разговора (пустой — окна нет: фотография по
	 * центру экрана).
	 */
	inline FPhotoPlacement PlacePhotoAboveDialog(const FVector2D& Screen, const FBox2D& DialogBox, float Aspect,
		float MinHeightFraction, float Margin)
	{
		FPhotoPlacement Result;
		const double SafeAspect = Aspect > 0.01f ? static_cast<double>(Aspect) : 1.5;
		const double M = FMath::Max(0.0, static_cast<double>(Margin));
		const double MinH = FMath::Clamp(static_cast<double>(MinHeightFraction), 0.05, 0.9) * Screen.Y;
		const double MaxW = Screen.X - 2.0 * M;
		if (Screen.X <= 1.0 || Screen.Y <= 1.0 || MaxW <= 1.0)
		{
			return Result;
		}

		double H;
		if (DialogBox.bIsValid)
		{
			const double Free = DialogBox.Min.Y - 2.0 * M; // место над окном разговора
			H = FMath::Max(Free, MinH);
		}
		else
		{
			H = Screen.Y - 2.0 * M;
		}
		H = FMath::Min(H, Screen.Y - 2.0 * M);
		double W = H * SafeAspect;
		if (W > MaxW)
		{
			W = MaxW;
			H = W / SafeAspect;
		}
		if (H <= 1.0)
		{
			return Result;
		}

		const double Top = DialogBox.bIsValid ? M : (Screen.Y - H) * 0.5;
		const double Left = (Screen.X - W) * 0.5;
		Result.Photo = FBox2D(FVector2D(Left, Top), FVector2D(Left + W, Top + H));
		if (DialogBox.bIsValid && Top + H + M > DialogBox.Min.Y)
		{
			Result.DialogTop = Top + H + M; // окно разговора опускается под фотографию
		}
		return Result;
	}
}
