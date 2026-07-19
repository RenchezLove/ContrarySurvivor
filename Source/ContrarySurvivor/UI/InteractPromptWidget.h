// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/UI/SelfHidingWidget.h"
#include "InteractPromptWidget.generated.h"

class UTextBlock;

/**
 * Контекстная подсказка взаимодействия на UMG (ADR-048, этап 3): «E — подобрать» /
 * «E — торговать» / «E — поговорить» внизу по центру. Раскладку WBP_InteractPrompt
 * строит Ринат. Живёт на экране всю игру (создаёт HUD в BeginPlay); сам прячется,
 * когда рядом нет интерактива или открыт модальный экран — через SetContentVisible
 * базы (Collapsed на самом виджете остановил бы его тик навсегда, баг смоука 07-18).
 * Тексты — существующие EditAnywhere-поля контроллера (InteractPromptPickup/Trader/Elder).
 * Локализация (ADR-050): панель ничего не склеивает, берёт готовый переводимый текст
 * методом GetInteractPromptDisplayText — он же выбирает вариант с клавишей (ПК) или без
 * неё (показан тач-слой).
 */
UCLASS()
class CONTRARYSURVIVOR_API UInteractPromptWidget : public USelfHidingWidget
{
	GENERATED_BODY()

protected:
	// Каждый кадр: текст/видимость от контроллера (перенос Canvas-поведения DrawHUD).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Кубики WBP_InteractPrompt (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	// Текст подсказки («E — подобрать» и т.п. — из полей контроллера). Ставит код.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PromptText;
};
