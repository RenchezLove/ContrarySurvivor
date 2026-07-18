// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractPromptWidget.generated.h"

class UTextBlock;

/**
 * Контекстная подсказка взаимодействия на UMG (ADR-048, этап 3): «E — подобрать» /
 * «E — торговать» / «E — поговорить» внизу по центру. Раскладку WBP_InteractPrompt
 * строит Ринат. Живёт на экране всю игру (создаёт HUD в BeginPlay); сам прячется,
 * когда рядом нет интерактива или открыт модальный экран. Тексты — существующие
 * EditAnywhere-поля контроллера (InteractPromptPickup/Trader/Elder).
 */
UCLASS()
class CONTRARYSURVIVOR_API UInteractPromptWidget : public UUserWidget
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
