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

	// ТЗ Рината 08-07, детект как в EndOfStoryWidget.cpp: дерево владельца из WBP уже
	// построено и кубики привязаны — строить и стилизовать ничего не нужно.
	bDesignerTree = (WidgetTree->RootWidget != nullptr);
	if (bDesignerTree)
	{
		if (!HintText)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("OnboardingHintWidget: в WBP_OnboardingHint нет кубика HintText — подсказки останутся без текста"));
		}
		return;
	}

	BuildCodeTree();
	ApplyStyle(FOnboardingHintStyle());
}

void UOnboardingHintWidget::BuildCodeTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HintRoot"));
	WidgetTree->RootWidget = Root;

	// Плашка в палитре подсказки взаимодействия Canvas-HUD (тёмный фон, тёплый жёлтый текст).
	// Значения — дефолты FOnboardingHintStyle; фактический стиль перекрывает ApplyStyle
	// (EditAnywhere-настройка на UOnboardingComponent).
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
}

void UOnboardingHintWidget::ApplyStyle(const FOnboardingHintStyle& Style)
{
	// Дерево владельца из WBP_OnboardingHint: плашка/шрифт/позиция — его, код не трогает
	// (ТЗ Рината 08-07). Текст подсказки ставит SetHintText в обоих путях.
	if (bDesignerTree)
	{
		return;
	}

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

void UOnboardingHintWidget::SetHintText(const FText& Text)
{
	if (HintText)
	{
		HintText->SetText(Text);
	}
}
