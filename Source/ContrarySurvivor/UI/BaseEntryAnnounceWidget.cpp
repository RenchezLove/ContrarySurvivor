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

	// Дерево пришло из АССЕТА (WBP_BaseAnnounce) — кубики уже привязаны BindWidgetOptional,
	// строить ничего не нужно: раскладка и стиль дизайнерские (П.0 ADR-077).
	if (WidgetTree->RootWidget)
	{
		SetVisibility(ESlateVisibility::Collapsed); // до первого показа
		return;
	}

	// ЗАПАСНОЙ режим без ассета — кодовое дерево с константным стилем:
	// канва на весь экран, блок — верхняя треть, по центру ширины.
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("AnnounceRoot"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// Цифра — ПЕРВОЙ в канву (рисуется под строкой: «вторым планом», ТЗ §5). Стиль —
	// константы запасного режима (в ассете стиль дизайнерский).
	DigitText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DigitText"));
	DigitText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DigitText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 96));
	DigitText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.18f)));
	if (UCanvasPanelSlot* DigitSlot = RootCanvas->AddChildToCanvas(DigitText))
	{
		DigitSlot->SetAnchors(FAnchors(0.5f, 0.22f, 0.5f, 0.22f));
		DigitSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		DigitSlot->SetAutoSize(true);
		DigitSlot->SetPosition(FVector2D(0.0f, 0.0f));
	}

	LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LineText"));
	LineText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	LineText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
	LineText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f)));
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

void UBaseEntryAnnounceWidget::ShowAnnounce(const FText& Line, const FText& InDigitText,
	bool bShowDigit, float Duration)
{
	if (!LineText)
	{
		return;
	}

	// Только тексты и видимость (П.0 ADR-077): стиль — дизайнерский.
	LineText->SetText(Line);
	if (DigitText)
	{
		DigitText->SetText(InDigitText);
		DigitText->SetVisibility(bShowDigit
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

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
