// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MockAdWidget.generated.h"

class UTextBlock;
class UButton;

/**
 * Экран-заглушка rewarded-ролика (Build 1.2, указание Рината): тёмный полноэкранный фон,
 * надпись «Здесь будет рекламный ролик», кнопка закрытия появляется через CloseDelay секунд
 * (до того — обратный отсчёт). Закрытие = «ролик досмотрен» (OnClosed -> onSuccess у
 * UMockAdService). Никакого реального SDK.
 *
 * ТЗ Рината 08-07 — два пути, как у UEndOfStoryWidget (архитектура ADR-048):
 *  - создан из WBP_MockAd (слот MockAdWidgetClass на HUD) → дерево владельца из дизайнера,
 *    кубики по BindWidgetOptional-именам; строку отсчёта и появление кнопки ведёт код;
 *  - ассета нет / слот пуст → прежний кодовый вид (BuildCodeTree).
 *
 * Отсчёт — в NativeTick по дельте Slate: игра на время «ролика» стоит на паузе
 * (ТЗ раздел 0 п.7), таймеры мира стоят вместе с ней, а Slate тикает всегда.
 */
UCLASS()
class CONTRARYSURVIVOR_API UMockAdWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Сколько секунд «идёт ролик» до появления кнопки закрытия (указание Рината: 3).
	float CloseDelay = 3.0f;

	// Кнопка закрытия нажата («ролик досмотрен»). Владелец (UMockAdService) снимает паузу
	// и зовёт onSuccess. Виджет сам убирает себя из viewport.
	FSimpleMulticastDelegate OnClosed;

	// Подпись placement-ключа мелким шрифтом (диагностика: какая точка показала ролик).
	void SetPlacement(FName Placement);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandleCloseClicked();

private:
	// Строит прежнее кодовое дерево (путь «ассета нет»). Имена кубиков = именам полей —
	// те же, что генерирует коммандлет в WBP_MockAd.
	void BuildCodeTree();

	// --- Кубики: из WBP по BindWidgetOptional ЛИБО из BuildCodeTree (имена совпадают) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlacementText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CountdownText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	// Прошедшее время «ролика» (сек, копится дельтой Slate — идёт и на паузе игры).
	float Elapsed = 0.0f;

	bool bCloseShown = false;

	// Дерево пришло из WBP-ассета (детект в NativeOnInitialized, как TouchControlsWidget.cpp).
	bool bDesignerTree = false;
};
