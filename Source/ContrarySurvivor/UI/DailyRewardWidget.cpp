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

	// Двойная рамка в палитре HUD: снаружи золотой кант, внутри тёмная панель (как модалки Canvas-HUD).
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DailyFrame"));
	Frame->SetBrushColor(FLinearColor(0.8f, 0.65f, 0.25f, 0.9f));
	Frame->SetPadding(FMargin(2.0f));

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DailyPanel"));
	Panel->SetBrushColor(FLinearColor(0.06f, 0.07f, 0.09f, 0.95f));
	Panel->SetPadding(FMargin(28.0f, 22.0f));
	Frame->SetContent(Panel);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DailyColumn"));
	Panel->SetContent(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DailyTitle"));
	Title->SetText(FText::FromString(TEXT("ЕЖЕДНЕВНАЯ НАГРАДА")));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 22));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f)));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	StreakText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DailyStreak"));
	StreakText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 17));
	StreakText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.96f, 1.0f, 1.0f)));
	if (UVerticalBoxSlot* StreakSlot = Column->AddChildToVerticalBox(StreakText))
	{
		StreakSlot->SetHorizontalAlignment(HAlign_Center);
		StreakSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	RewardText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DailyReward"));
	RewardText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 26));
	RewardText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.2f, 1.0f)));
	if (UVerticalBoxSlot* RewardSlot = Column->AddChildToVerticalBox(RewardText))
	{
		RewardSlot->SetHorizontalAlignment(HAlign_Center);
		RewardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	UButton* TakeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DailyTake"));
	TakeButton->OnClicked.AddDynamic(this, &UDailyRewardWidget::HandleTakeClicked);

	UTextBlock* TakeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DailyTakeLabel"));
	TakeLabel->SetText(FText::FromString(TEXT("Забрать")));
	TakeLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
	TakeLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f)));
	TakeButton->SetContent(TakeLabel);

	if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(TakeButton))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	}

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Frame))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.42f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}
}

void UDailyRewardWidget::SetupContent(int32 StreakDays, float RewardAmount)
{
	if (StreakText)
	{
		StreakText->SetText(FText::FromString(FString::Printf(TEXT("День серии: %d"), StreakDays)));
	}
	if (RewardText)
	{
		RewardText->SetText(FText::FromString(FString::Printf(TEXT("+%.0f монет"), RewardAmount)));
	}
}

void UDailyRewardWidget::HandleTakeClicked()
{
	OnClosed.Broadcast();
	RemoveFromParent();
}
