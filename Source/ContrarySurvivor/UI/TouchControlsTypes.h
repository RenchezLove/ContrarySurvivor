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
};
