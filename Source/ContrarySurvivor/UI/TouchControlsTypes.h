// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TouchControlsTypes.generated.h"

/**
 * Настройки одной экранной тач-кнопки (этап G). Живут UPROPERTY на контроллере (директива
 * Рината: весь тюнинг в редакторе без перекомпиляции), копируются в UTouchControlsWidget
 * при создании слоя. К какому углу экрана кнопка прижата — задаёт код виджета, не настройка.
 */
USTRUCT(BlueprintType)
struct FTouchButtonSettings
{
	GENERATED_BODY()

	// Показывать ли кнопку (ненужную можно спрятать без перекомпиляции).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls")
	bool bEnabled = true;

	// Отступ ЦЕНТРА кнопки от СВОЕГО угла экрана, px: X — от бокового края, Y — от верхнего/нижнего.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls")
	FVector2D Margin = FVector2D(120.0f, 120.0f);

	// Радиус кнопки, px.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (ClampMin = "15.0"))
	float Radius = 55.0f;

	// Подпись на кнопке (пусто = без подписи). Дефолты по кнопкам задаёт контроллер.
	// Переводимый текст (ADR-050); действует только для дерева, построенного кодом —
	// у WBP-кнопок подпись своя, из дизайнера, и код её не трогает.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls")
	FText Label;

	// Тон кнопки. Альфа умножается на штатные прозрачности состояний (норма 0.30 /
	// под пальцем 0.55 и т.д.) — белый с альфой 1 даёт прежний вид.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls")
	FLinearColor Color = FLinearColor::White;

	// Цвет текста подписи.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls")
	FLinearColor TextColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	// Размер шрифта подписи; 0 = автоматически от радиуса (Radius*0.3, кламп 10..22).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (ClampMin = "0"))
	int32 FontSize = 0;
};
