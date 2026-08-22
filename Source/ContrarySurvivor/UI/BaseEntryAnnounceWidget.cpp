// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/BaseEntryAnnounceWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UBaseEntryAnnounceWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	// Кодовое дерево: канва на весь экран, блок — верхняя треть, по центру ширины.
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AnnounceRoot"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// Цифра — ПЕРВОЙ в канву (рисуется под строкой: «вторым планом», ТЗ §5).
	DigitText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AnnounceDigit"));
	DigitText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* DigitSlot = RootCanvas->AddChildToCanvas(DigitText))
	{
		DigitSlot->SetAnchors(FAnchors(0.5f, 0.22f, 0.5f, 0.22f));
		DigitSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		DigitSlot->SetAutoSize(true);
		DigitSlot->SetPosition(FVector2D(0.0f, 0.0f));
	}

	LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AnnounceLine"));
	LineText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* LineSlot = RootCanvas->AddChildToCanvas(LineText))
	{
		LineSlot->SetAnchors(FAnchors(0.5f, 0.22f, 0.5f, 0.22f));
		LineSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		LineSlot->SetAutoSize(true);
		LineSlot->SetPosition(FVector2D(0.0f, 0.0f));
	}

	// До первого показа надпись скрыта.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UBaseEntryAnnounceWidget::ShowAnnounce(const FText& Line, const FText& InDigitText, bool bShowDigit,
	int32 LineFontSize, const FLinearColor& LineColor,
	int32 DigitFontSize, const FLinearColor& DigitColor, float Duration)
{
	if (!LineText || !DigitText)
	{
		return;
	}

	LineText->SetText(Line);
	LineText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", LineFontSize));
	LineText->SetColorAndOpacity(FSlateColor(LineColor));

	DigitText->SetText(InDigitText);
	DigitText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", DigitFontSize));
	DigitText->SetColorAndOpacity(FSlateColor(DigitColor));
	DigitText->SetVisibility(bShowDigit
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// Повторный показ перезапускает таймер скрытия.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HideTimerHandle, this,
			&UBaseEntryAnnounceWidget::HideAnnounce, FMath::Max(0.5f, Duration), /*bLoop=*/false);
	}
}

void UBaseEntryAnnounceWidget::HideAnnounce()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
