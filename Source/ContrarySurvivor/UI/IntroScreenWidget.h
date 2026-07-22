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
 * затем плавное проявление мира. Дерево строится КОДОМ (как UTouchControlsWidget) — без WBP и без
 * зависимости от unreal-operator. Таймингом и альфами управляет контроллер (UpdateIntro): фон и
 * строка гаснут/проступают независимыми прозрачностями. Слой не перехватывает ввод
 * (HitTestInvisible) — пропуск интро контроллер ловит опросом клавиш, а не фокусом виджета.
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
	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	// Сплошной чёрный фон на весь экран (процедурная кисть — без текстуры).
	UPROPERTY()
	TObjectPtr<UImage> Background;

	// Крупная строка по центру.
	UPROPERTY()
	TObjectPtr<UTextBlock> LineText;

	// Мелкая подсказка пропуска внизу.
	UPROPERTY()
	TObjectPtr<UTextBlock> SkipHintText;
};
