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
 * П.0 отчёта Рината 23.08 (ADR-077): раскладка и стиль — В АССЕТЕ WBP_BaseAnnounce
 * (создаёт генератор, кубики LineText/DigitText по BindWidgetOptional, Ринат двигает и
 * стилизует мышкой; класс окна назначается на базе слотом AnnounceWidgetClass). Код ставит
 * ТОЛЬКО тексты и видимость. Кодовое дерево ниже — ЗАПАСНОЙ режим, когда виджет создан
 * голым C++-классом без ассета (пока WBP не сгенерирован/не назначен).
 *
 * Скрытие — по таймеру; свой тик виджету не нужен, поэтому Collapsed на самом виджете
 * безопасен (ловушка SelfHiding — про убитый NativeTick, которого здесь нет).
 */
UCLASS()
class CONTRARYSURVIVOR_API UBaseEntryAnnounceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Показать надпись: Line — готовая строка, InDigitText — номер ступени (виден при
	// bShowDigit). Стиль — дизайнерский (ассет); код текстов и видимости не превышает.
	// Повторный вызов перезапускает таймер скрытия.
	void ShowAnnounce(const FText& Line, const FText& InDigitText, bool bShowDigit, float Duration);

protected:
	virtual void NativeOnInitialized() override;

	// Крупная полупрозрачная цифра ступени (вторым планом — ПОД строкой). В ассете кубик
	// зовётся DigitText; без ассета создаётся запасным кодовым деревом.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DigitText;

	// Строка надписи.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LineText;

private:
	FTimerHandle HideTimerHandle;

	void HideAnnounce();
};
