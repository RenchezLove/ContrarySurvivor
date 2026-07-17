// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/OnboardingHintWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UOnboardingHintWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HintRoot"));
	WidgetTree->RootWidget = Root;

	// Плашка в палитре подсказки взаимодействия Canvas-HUD (тёмный фон, тёплый жёлтый текст).
	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HintPlate"));
	Plate->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
	Plate->SetPadding(FMargin(18.0f, 12.0f));

	HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintText"));
	HintText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
	HintText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.95f, 0.5f, 1.0f)));
	HintText->SetAutoWrapText(true);
	HintText->SetJustification(ETextJustify::Center);
	Plate->SetContent(HintText);

	if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(Plate))
	{
		// Фиксированная ширина + авто-перенос текста: длинная подсказка (инвентарь) уходит
		// на вторую строку, а не за край экрана.
		PlateSlot->SetAnchors(FAnchors(0.5f, 0.10f));
		PlateSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		PlateSlot->SetAutoSize(false);
		PlateSlot->SetPosition(FVector2D::ZeroVector);
		PlateSlot->SetSize(FVector2D(820.0f, 96.0f));
	}
}

void UOnboardingHintWidget::SetHintText(const FString& Text)
{
	if (HintText)
	{
		HintText->SetText(FText::FromString(Text));
	}
}
