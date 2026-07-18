// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/InteractPromptWidget.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "Components/TextBlock.h"

void UInteractPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetOwningPlayer());

	// Прячемся без интерактива рядом и на модальных экранах (перенос Canvas-поведения).
	// Именно содержимое, не сам виджет: Collapsed на себе убил бы собственный тик.
	if (!PC || !PC->HasInteractPrompt() || PC->IsAnyModalUIOpen())
	{
		SetContentVisible(false);
		return;
	}
	SetContentVisible(true);

	if (PromptText)
	{
		PromptText->SetText(FText::FromString(PC->GetInteractPromptText()));
	}
}
