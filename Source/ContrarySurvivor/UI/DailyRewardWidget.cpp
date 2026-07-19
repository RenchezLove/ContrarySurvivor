// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/DailyRewardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Styling/CoreStyle.h"

void UDailyRewardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	// Корень — канвас на весь экран; панель по центру, чуть выше середины (не спорит с HUD-статами).
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DailyRoot"));
	WidgetTree->RootWidget = Root;

	// Двойная рамка в палитре HUD: снаружи золотой кант, внутри тёмная панель (как модалки
	// Canvas-HUD). Цвета/тексты/шрифты — дефолты FDailyRewardStyle; фактический стиль
	// перекрывает ApplyStyle (EditAnywhere-настройка UDailyRewardComponent).
	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DailyFrame"));
	FrameBorder->SetPadding(FMargin(2.0f));

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DailyPanel"));
	PanelBorder->SetPadding(FMargin(28.0f, 22.0f));
	FrameBorder->SetContent(PanelBorder);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DailyColumn"));
	PanelBorder->SetContent(Column);

	TitleBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DailyTitle"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleBlock))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	StreakText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DailyStreak"));
	if (UVerticalBoxSlot* StreakSlot = Column->AddChildToVerticalBox(StreakText))
	{
		StreakSlot->SetHorizontalAlignment(HAlign_Center);
		StreakSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	RewardText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DailyReward"));
	if (UVerticalBoxSlot* RewardSlot = Column->AddChildToVerticalBox(RewardText))
	{
		RewardSlot->SetHorizontalAlignment(HAlign_Center);
		RewardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	UButton* TakeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DailyTake"));
	TakeButton->OnClicked.AddDynamic(this, &UDailyRewardWidget::HandleTakeClicked);

	TakeLabelBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DailyTakeLabel"));
	TakeButton->SetContent(TakeLabelBlock);

	if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(TakeButton))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	}

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(FrameBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.42f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}

	ApplyStyle(CurrentStyle);
}

void UDailyRewardWidget::ApplyStyle(const FDailyRewardStyle& Style)
{
	CurrentStyle = Style; // SetupContent берёт отсюда форматы строк

	if (FrameBorder) { FrameBorder->SetBrushColor(Style.FrameColor); }
	if (PanelBorder) { PanelBorder->SetBrushColor(Style.PanelColor); }
	if (TitleBlock)
	{
		TitleBlock->SetText(Style.TitleText);
		TitleBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
		TitleBlock->SetColorAndOpacity(FSlateColor(Style.TitleColor));
	}
	if (StreakText)
	{
		StreakText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.StreakFontSize)));
		StreakText->SetColorAndOpacity(FSlateColor(Style.StreakColor));
	}
	if (RewardText)
	{
		RewardText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.RewardFontSize)));
		RewardText->SetColorAndOpacity(FSlateColor(Style.RewardColor));
	}
	if (TakeLabelBlock)
	{
		TakeLabelBlock->SetText(Style.TakeButtonText);
		TakeLabelBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TakeButtonFontSize)));
		TakeLabelBlock->SetColorAndOpacity(FSlateColor(Style.TakeButtonTextColor));
	}
}

void UDailyRewardWidget::SetupContent(int32 StreakDays, float RewardAmount)
{
	// Сборка по форматам стиля: «День серии: 3» / «+35 монет». Подстановки именованные,
	// числа через FText::AsNumber — порядок слов задаёт перевод, а не код (ADR-050).
	if (StreakText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Days"), FText::AsNumber(StreakDays));
		StreakText->SetText(FText::Format(CurrentStyle.StreakFormat, Args));
	}
	if (RewardText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Amount"), FText::AsNumber(FMath::RoundToInt32(RewardAmount)));
		RewardText->SetText(FText::Format(CurrentStyle.RewardFormat, Args));
	}
}

void UDailyRewardWidget::HandleTakeClicked()
{
	OnClosed.Broadcast();
	RemoveFromParent();
}
