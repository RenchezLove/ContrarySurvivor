// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Дроссель «громкого маяка» рассинхрона оружия (правка 07-08 по жалобе Рината: Warning
 * «CurrentWeapon is ARangedWeapon, but != RangedWeaponInstance» писался КАЖДЫЙ кадр из
 * NativeTick обоих виджетов и заспамливал лог). Правило: писать один раз при ВХОДЕ в
 * состояние рассинхрона; вышли из него — флаг сбрасывается, следующий вход снова даст
 * ровно одну строку. Чистая функция без UObject — гоняется автотестом без редактора
 * (паттерн Retention/DailyRewardLogic.h).
 */
namespace WeaponUiSyncLog
{
	// bDesyncNow — рассинхрон наблюдается в этом кадре; bInOutLogged — флаг «уже писали»
	// (член виджета). Возвращает true ровно один раз на каждый непрерывный эпизод рассинхрона.
	inline bool ShouldLogDesyncOnce(bool bDesyncNow, bool& bInOutLogged)
	{
		if (bDesyncNow)
		{
			if (bInOutLogged)
			{
				return false;
			}
			bInOutLogged = true;
			return true;
		}
		bInOutLogged = false;
		return false;
	}
}
