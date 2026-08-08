// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/IntroScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

void UIntroScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	// Слой не должен перехватывать ввод (пропуск ловится опросом клавиш контроллером).
	SetVisibility(ESlateVisibility::HitTestInvisible);

	// ТЗ Рината 08-07, детект как в EndOfStoryWidget.cpp: дерево владельца из WBP уже
	// построено и кубики (Background/LineText/SkipHintText) привязаны — строить нечего,
	// шрифты и позиции строк за владельцем; альфы и тексты дальше ведёт контроллер.
	bDesignerTree = (WidgetTree->RootWidget != nullptr);
	if (bDesignerTree)
	{
		if (!Background || !LineText)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("IntroScreenWidget: в WBP_Intro нет кубика Background или LineText — интро останется без этого элемента"));
		}
		return;
	}

	BuildCodeTree();
}

void UIntroScreenWidget::BuildCodeTree()
{
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("IntroRoot"));
	WidgetTree->RootWidget = RootCanvas;

	// Чёрный фон на весь экран. Процедурная кисть RoundedBox с нулевым скруглением рисует
	// сплошной прямоугольник без текстуры (тот же приём, что круглые кнопки тач-слоя).
	Background = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Background"));
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(FLinearColor::Black);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(0.0, 0.0, 0.0, 0.0);
		Background->SetBrush(Brush);
	}
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* BgSlot = RootCanvas->AddChildToCanvas(Background))
	{
		BgSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f)); // растянуть на весь экран
		BgSlot->SetOffsets(FMargin(0.0f));
	}

	// Крупная строка по центру. Центральная полоса ~70% ширины, авто-перенос длинных строк.
	LineText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LineText"));
	LineText->SetJustification(ETextJustify::Center);
	LineText->SetAutoWrapText(true);
	LineText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LineText->SetVisibility(ESlateVisibility::HitTestInvisible);
	{
		FSlateFontInfo Font = LineText->GetFont();
		Font.Size = 42;
		LineText->SetFont(Font);
	}
	if (UCanvasPanelSlot* LineSlot = RootCanvas->AddChildToCanvas(LineText))
	{
		LineSlot->SetAnchors(FAnchors(0.15f, 0.42f, 0.85f, 0.58f)); // центральная полоса
		LineSlot->SetOffsets(FMargin(0.0f));
	}

	// Подсказка пропуска внизу (мелкая, светло-серая). По умолчанию скрыта.
	SkipHintText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SkipHintText"));
	SkipHintText->SetJustification(ETextJustify::Center);
	SkipHintText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.82f, 1.0f)));
	SkipHintText->SetVisibility(ESlateVisibility::Collapsed);
	{
		FSlateFontInfo Font = SkipHintText->GetFont();
		Font.Size = 22;
		SkipHintText->SetFont(Font);
	}
	if (UCanvasPanelSlot* HintSlot = RootCanvas->AddChildToCanvas(SkipHintText))
	{
		HintSlot->SetAnchors(FAnchors(0.2f, 0.88f, 0.8f, 0.96f));
		HintSlot->SetOffsets(FMargin(0.0f));
	}
}

void UIntroScreenWidget::SetBackgroundAlpha(float Alpha)
{
	if (Background)
	{
		Background->SetRenderOpacity(FMath::Clamp(Alpha, 0.0f, 1.0f));
	}
}

void UIntroScreenWidget::SetLineText(const FText& Text)
{
	if (LineText)
	{
		LineText->SetText(Text);
	}
}

void UIntroScreenWidget::SetTextAlpha(float Alpha)
{
	if (LineText)
	{
		LineText->SetRenderOpacity(FMath::Clamp(Alpha, 0.0f, 1.0f));
	}
}

void UIntroScreenWidget::SetSkipHint(const FText& Text, bool bVisible)
{
	if (SkipHintText)
	{
		SkipHintText->SetText(Text);
		SkipHintText->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}
