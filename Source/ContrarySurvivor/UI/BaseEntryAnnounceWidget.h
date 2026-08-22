// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/TimerHandle.h"
#include "BaseEntryAnnounceWidget.generated.h"

class UTextBlock;

/**
 * Надпись при входе на базу противника (ТЗ издателя 22.08.2026 §5): строка вида
 * «Лагерь бандитов. Эти выглядят ещё более опытными», а с третьей ступени рядом — номер
 * ступени «полупрозрачной цифрой, без скобок, вторым планом».
 *
 * Кодовое дерево БЕЗ ассета (паттерн кодовых окон этапа F): канва на весь экран, крупная
 * полупрозрачная цифра ПОЗАДИ строки (добавлена в канву первой — рисуется под ней), строка
 * поверх; блок в верхней трети экрана, тапы сквозь (SelfHitTestInvisible). Все тексты,
 * кегли и цвета приходят ОТ БАЗЫ при каждом показе (настройки — на AMasterEnemyBase,
 * EditAnywhere в BP и на экземпляре — требование ТЗ). Скрытие — по таймеру; свой тик
 * виджету не нужен, поэтому Collapsed на самом виджете безопасен (ловушка SelfHiding —
 * про убитый NativeTick, которого здесь нет).
 */
UCLASS()
class CONTRARYSURVIVOR_API UBaseEntryAnnounceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Показать надпись: Line — готовая строка, DigitText — номер ступени (показывается при
	// bShowDigit), стиль — от базы. Повторный вызов перезапускает таймер скрытия.
	void ShowAnnounce(const FText& Line, const FText& DigitText, bool bShowDigit,
		int32 LineFontSize, const FLinearColor& LineColor,
		int32 DigitFontSize, const FLinearColor& DigitColor, float Duration);

protected:
	virtual void NativeOnInitialized() override;

private:
	// Крупная полупрозрачная цифра ступени (вторым планом — ПОД строкой).
	UPROPERTY()
	TObjectPtr<UTextBlock> DigitText;

	// Строка надписи.
	UPROPERTY()
	TObjectPtr<UTextBlock> LineText;

	FTimerHandle HideTimerHandle;

	void HideAnnounce();
};
