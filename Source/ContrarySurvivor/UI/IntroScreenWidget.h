// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "IntroScreenWidget.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;

/**
 * Экран интро (Build 1, ТЗ издателя раздел 2): чёрный экран с двумя короткими строками по центру,
 * затем плавное проявление мира. Таймингом и альфами управляет контроллер (UpdateIntro): фон и
 * строка гаснут/проступают независимыми прозрачностями. Слой не перехватывает ввод
 * (HitTestInvisible) — пропуск интро контроллер ловит опросом клавиш, а не фокусом виджета.
 *
 * ТЗ Рината 08-07 — два пути, как у UEndOfStoryWidget (архитектура ADR-048):
 *  - создан из WBP_Intro (слот IntroScreenWidgetClass на контроллере) → дерево владельца из
 *    дизайнера (шрифт/позиции строк правятся мышкой), кубики по BindWidgetOptional-именам;
 *    тексты строк и прозрачности по-прежнему ведёт код — это и есть интро-анимация;
 *  - ассета нет / слот пуст → прежний кодовый вид (BuildCodeTree).
 */
UCLASS()
class CONTRARYSURVIVOR_API UIntroScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Прозрачность чёрного фона: 1 = сплошной чёрный экран, 0 = мир полностью виден.
	void SetBackgroundAlpha(float Alpha);

	// Текущая строка по центру экрана.
	void SetLineText(const FText& Text);

	// Прозрачность строки (проступает и гаснет отдельно от фона).
	void SetTextAlpha(float Alpha);

	// Подсказка «зажми, чтобы пропустить» внизу экрана (только при повторных заходах).
	void SetSkipHint(const FText& Text, bool bVisible);

protected:
	virtual void NativeOnInitialized() override;

private:
	// Строит прежнее кодовое дерево (путь «ассета нет»). Имена кубиков = именам полей —
	// те же, что генерирует коммандлет в WBP_Intro.
	void BuildCodeTree();

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	// --- Кубики: из WBP по BindWidgetOptional ЛИБО из BuildCodeTree (имена совпадают) ---

	// Сплошной чёрный фон на весь экран (процедурная кисть — без текстуры).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Background;

	// Крупная строка по центру.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LineText;

	// Мелкая подсказка пропуска внизу.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SkipHintText;

	// Дерево пришло из WBP-ассета (детект в NativeOnInitialized, как TouchControlsWidget.cpp).
	bool bDesignerTree = false;
};
