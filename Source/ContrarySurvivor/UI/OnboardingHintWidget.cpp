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
	// Значения — дефолты FOnboardingHintStyle; фактический стиль перекрывает ApplyStyle
	// (EditAnywhere-настройка на UOnboardingComponent).
	const FOnboardingHintStyle Defaults;

	HintPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("HintPlate"));
	HintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HintText"));
	HintText->SetAutoWrapText(true);
	HintText->SetJustification(ETextJustify::Center);
	HintPlate->SetContent(HintText);

	if (UCanvasPanelSlot* PlateSlot = Root->AddChildToCanvas(HintPlate))
	{
		// Фиксированная ширина + авто-перенос текста: длинная подсказка (инвентарь) уходит
		// на вторую строку, а не за край экрана. Выравнивание — верх-центр.
		PlateSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		PlateSlot->SetAutoSize(false);
		PlateSlot->SetPosition(FVector2D::ZeroVector);
	}

	ApplyStyle(Defaults);
}

void UOnboardingHintWidget::ApplyStyle(const FOnboardingHintStyle& Style)
{
	if (HintPlate)
	{
		HintPlate->SetBrushColor(Style.PlateColor);
		HintPlate->SetPadding(FMargin(Style.PlatePadding.X, Style.PlatePadding.Y));
		if (UCanvasPanelSlot* PlateSlot = Cast<UCanvasPanelSlot>(HintPlate->Slot))
		{
			PlateSlot->SetAnchors(FAnchors(Style.ScreenAnchor.X, Style.ScreenAnchor.Y));
			PlateSlot->SetSize(Style.BoxSize);
		}
	}
	if (HintText)
	{
		HintText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.FontSize)));
		HintText->SetColorAndOpacity(FSlateColor(Style.TextColor));
	}
}

void UOnboardingHintWidget::SetHintText(const FString& Text)
{
	if (HintText)
	{
		HintText->SetText(FText::FromString(Text));
	}
}
