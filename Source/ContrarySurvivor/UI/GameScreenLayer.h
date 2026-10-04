// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

/**
 * Единая дверь «поставить окно на экран» (решение Рината 04.10.2026 про рекламный баннер:
 * игра сжимается и освобождает под баннер полосу снизу — см. Ads/AdGameArea.h).
 *
 * ⛔ Окна проекта добавлять ТОЛЬКО через AddToGameScreen, а не через AddToViewport. Разница
 * принципиальна: AddToViewport кладёт окно на весь экран поверх всего, и при сжатой игре
 * оно осталось бы на полном экране — под баннером. AddToPlayerScreen кладёт окно в область
 * вывода игрока, которая сжимается вместе с игрой (движок раскладывает её по ULocalPlayer::
 * Size/Origin — SGameLayerManager.cpp:630-646, UE 5.5), вместе с зонами нажатий.
 *
 * Исключение — то, что обязано лежать на ПОЛНОМ экране: заглушки самой рекламы
 * (UMockAdService: «ролик» и серая полоса баннера).
 */
namespace ContraryUI
{
	inline void AddToGameScreen(UUserWidget* Widget, int32 ZOrder = 0)
	{
		if (!Widget)
		{
			return;
		}
		// Окно без игрока-владельца (служебный мир) на экран игрока не встанет — ему остаётся
		// полный экран, как раньше.
		if (Widget->GetOwningLocalPlayer())
		{
			Widget->AddToPlayerScreen(ZOrder);
		}
		else
		{
			Widget->AddToViewport(ZOrder);
		}
	}
}
