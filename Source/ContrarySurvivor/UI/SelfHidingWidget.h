// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SelfHidingWidget.generated.h"

/**
 * База для виджетов, которые прячут СЕБЯ из собственного NativeTick (подсказка E,
 * трекер квеста). Прямой SetVisibility(Collapsed) на самом UUserWidget здесь запрещён:
 * Slate тикает только отрисовываемые виджеты, поэтому свернувший себя виджет теряет
 * NativeTick и НИКОГДА не разворачивается обратно (баг PIE-смоука 2026-07-18,
 * доказан экспериментом game-lead: принудительный показ оживлял тик до следующего
 * самосворачивания).
 *
 * Решение: сам UUserWidget всегда остаётся видимым для Slate (и тикает), а прячется
 * корень ЕГО ДЕРЕВА (WidgetTree->RootWidget) — содержимое исчезает целиком вместе с
 * плашками. Работает с любой раскладкой WBP: именованный контейнер не нужен, «показанная»
 * видимость корня запоминается с ассета и восстанавливается как была.
 */
UCLASS(Abstract)
class CONTRARYSURVIVOR_API USelfHidingWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	// Показать/спрятать содержимое. Дёргать из NativeTick можно каждый кадр —
	// повторный вызов с тем же состоянием бесплатен.
	void SetContentVisible(bool bVisible);

	bool IsContentVisible() const { return bContentVisible; }

private:
	bool bContentVisible = true;

	// «Показанная» видимость корня дерева — какой её задал ассет/код (у канвы обычно
	// SelfHitTestInvisible). Кэшируется при первом скрытии, чтобы не навязать своё.
	bool bShownVisibilityCached = false;
	ESlateVisibility ShownRootVisibility = ESlateVisibility::SelfHitTestInvisible;
};
