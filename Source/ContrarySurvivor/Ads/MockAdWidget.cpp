// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Ads/MockAdWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UMockAdWidget::NativeOnInitialized()
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
		struct { const UWidget* W; const TCHAR* Name; } Expected[] =
		{
			{ PlacementText, TEXT("PlacementText") },
			{ CountdownText, TEXT("CountdownText") },
			{ CloseButton, TEXT("CloseButton") },
		};
		for (const auto& Entry : Expected)
		{
			if (!Entry.W)
			{
				UE_LOG(LogQA, Warning,
					TEXT("MockAdWidget: кубик %s не найден в WBP_MockAd — элемент отключён"),
					Entry.Name);
			}
		}
	}
	else
	{
		BuildCodeTree();
	}

	// Клики и стартовые состояния — в обоих путях (в ассете кнопка видима, чтобы владельцу
	// было что редактировать; на живом экране её показывает конец отсчёта в NativeTick).
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UMockAdWidget::HandleCloseClicked);
		CloseButton->SetVisibility(ESlateVisibility::Collapsed); // до конца отсчёта кнопки нет
	}
}

void UMockAdWidget::BuildCodeTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MockAdRoot"));
	WidgetTree->RootWidget = Root;

	// Непрозрачный тёмный фон на весь экран: «ролик» перекрывает игру целиком.
	UBorder* Dim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	Dim->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.97f));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MockAdColumn"));
	if (UCanvasPanelSlot* ColumnSlot = Root->AddChildToCanvas(Column))
	{
		ColumnSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		ColumnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		ColumnSlot->SetAutoSize(true);
		ColumnSlot->SetPosition(FVector2D::ZeroVector);
	}

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	Title->SetText(NSLOCTEXT("MockAd", "Title", "Здесь будет рекламный ролик"));
	Title->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 30));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.96f, 1.0f, 1.0f)));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(Title))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	PlacementText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlacementText"));
	PlacementText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
	PlacementText->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.57f, 0.62f, 1.0f)));
	if (UVerticalBoxSlot* PlacementSlot = Column->AddChildToVerticalBox(PlacementText))
	{
		PlacementSlot->SetHorizontalAlignment(HAlign_Center);
		PlacementSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 26.0f));
	}

	CountdownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CountdownText"));
	CountdownText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 18));
	CountdownText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.85f, 1.0f)));
	if (UVerticalBoxSlot* CountdownSlot = Column->AddChildToVerticalBox(CountdownText))
	{
		CountdownSlot->SetHorizontalAlignment(HAlign_Center);
		CountdownSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));

	UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseText"));
	CloseLabel->SetText(NSLOCTEXT("MockAd", "Close", "  Закрыть  "));
	CloseLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
	CloseLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f)));
	CloseButton->SetContent(CloseLabel);
	if (UVerticalBoxSlot* CloseSlot = Column->AddChildToVerticalBox(CloseButton))
	{
		CloseSlot->SetHorizontalAlignment(HAlign_Center);
	}
}

void UMockAdWidget::SetPlacement(FName Placement)
{
	if (PlacementText)
	{
		PlacementText->SetText(FText::FromString(Placement.ToString()));
	}
}

void UMockAdWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bCloseShown)
	{
		return;
	}

	Elapsed += InDeltaTime;
	const float Remaining = CloseDelay - Elapsed;
	if (Remaining > 0.0f)
	{
		if (CountdownText)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Seconds"), FText::AsNumber(FMath::CeilToInt32(Remaining)));
			CountdownText->SetText(FText::Format(
				NSLOCTEXT("MockAd", "Countdown", "Ролик идёт… {Seconds}"), Args));
		}
		return;
	}

	bCloseShown = true;
	if (CountdownText)
	{
		CountdownText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (CloseButton)
	{
		CloseButton->SetVisibility(ESlateVisibility::Visible);
	}
}

void UMockAdWidget::HandleCloseClicked()
{
	UE_LOG(LogQA, Display, TEXT("QA: MOCK-AD closed by player (treated as watched)"));
	OnClosed.Broadcast();
	RemoveFromParent();
}
