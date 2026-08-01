// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
 * Чистая арифметика потерь при смерти (Build 1.2, переопределение Рината поверх ТЗ №1):
 * без рекламы теряется 70% расходников и 50% денег; со «Спасти рюкзак» (просмотр ролика) —
 * лишь 10% и того и другого. Из ПОТЕРЯННОГО половина падает мешком на месте гибели
 * (остальное исчезает). Все доли — EditAnywhere на APlayerCharacter, сюда приходят
 * параметрами. Без UObject/мира — гоняется headless-автотестом (Tests/AdsAutomationTests).
 *
 * Округление — FMath::RoundToInt32 (половина от нуля вверх): 70% от 1 предмета = потерян
 * 1; половина от 1 потерянного = падает 1. Пока экран смерти открыт, НИЧЕГО не списано —
 * план считается заранее и для показа игроку, и для применения по кнопке.
 */
namespace DeathLoss
{
	struct FPlan
	{
		// Build 1.2.1 (блок Г, стаки): счёт идёт ШТУКАМИ, не акторами — «70% расходников»
		// означает 70% суммы штук во всех стаках; стак теряет штуки уменьшением счётчика,
		// а не исчезает целиком. Раскладку штук по конкретным стакам даёт
		// SplitLossAcrossStacks (та же функция у превью экрана смерти и применения).

		// Сколько ШТУК расходников теряется (списываются со стаков рюкзака).
		int32 LostItems = 0;

		// Из потерянных штук — сколько падает мешком на месте гибели (первые по списку);
		// остальные (LostItems - DroppedItems) уничтожаются.
		int32 DroppedItems = 0;

		// Сколько денег теряется (списываются с баланса на момент смерти).
		float LostMoney = 0.0f;

		// Из потерянных денег — сколько ложится в мешок на месте гибели.
		float DroppedMoney = 0.0f;
	};

	// ItemCount — сумма ШТУК неэкипированных расходников рюкзака (по стакам); Money —
	// денег на момент смерти; ItemLossFrac/MoneyLossFrac — доли потери [0..1]; DropFrac —
	// доля потерянного, падающая мешком [0..1]. Доли клампятся, отрицательные входы дают
	// нулевой план.
	CONTRARYSURVIVOR_API FPlan Compute(int32 ItemCount, float Money,
		float ItemLossFrac, float MoneyLossFrac, float DropFrac);

	// Потеря одного стака в раскладке SplitLossAcrossStacks: сколько штук из него падает
	// мешком и сколько уничтожается (остальное остаётся в стаке).
	struct FStackLoss
	{
		int32 DroppedPieces = 0;
		int32 DestroyedPieces = 0;
	};

	// Build 1.2.1: раскладка потери ШТУК по стакам в порядке инвентаря. StackSizes —
	// размеры стаков кандидатов; теряются первые LostPieces штук, из них первые
	// DroppedPieces падают мешком. DroppedPieces клампится в [0..LostPieces]; мусорные
	// размеры (<0) читаются как 0. Результат — на каждый вход по элементу.
	CONTRARYSURVIVOR_API TArray<FStackLoss> SplitLossAcrossStacks(
		const TArray<int32>& StackSizes, int32 LostPieces, int32 DroppedPieces);
}
