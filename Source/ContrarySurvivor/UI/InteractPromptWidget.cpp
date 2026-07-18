// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/InteractPromptWidget.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "Components/TextBlock.h"

void UInteractPromptWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetOwningPlayer());

	// Прячемся без интерактива рядом и на модальных экранах (перенос Canvas-поведения).
	if (!PC || !PC->HasInteractPrompt() || PC->IsAnyModalUIOpen())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (PromptText)
	{
		PromptText->SetText(FText::FromString(PC->GetInteractPromptText()));
	}
}
