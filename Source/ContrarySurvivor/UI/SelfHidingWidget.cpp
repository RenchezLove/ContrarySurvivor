// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/SelfHidingWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"

void USelfHidingWidget::SetContentVisible(bool bVisible)
{
	if (bVisible == bContentVisible)
	{
		return;
	}
	UWidget* Root = WidgetTree ? WidgetTree->RootWidget.Get() : nullptr;
	if (!Root)
	{
		return;
	}
	if (!bShownVisibilityCached)
	{
		// Первый вызов всегда случается из «показанного» состояния (виджет создаётся
		// видимым) — текущая видимость корня и есть та, которую надо восстанавливать.
		ShownRootVisibility = Root->GetVisibility();
		bShownVisibilityCached = true;
	}
	Root->SetVisibility(bVisible ? ShownRootVisibility : ESlateVisibility::Collapsed);
	bContentVisible = bVisible;
}
