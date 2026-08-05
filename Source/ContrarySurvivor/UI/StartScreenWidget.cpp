// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/StartScreenWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Styling/CoreStyle.h"

void UStartScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StartRoot"));
	WidgetTree->RootWidget = Root;

	// Стиль (цвета/тексты/шрифты) — дефолты FStartScreenStyle; фактический стиль перекрывает
	// ApplyStyle (EditAnywhere-настройка контроллера, тот же паттерн, что у меню паузы).
	const FStartScreenStyle Defaults;

	// Затемнение на весь экран. Visible — ловит хит-тест, чтобы клик мимо кнопок не ушёл в мир.
	DimmerBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StartDimmer"));
	if (UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(DimmerBorder))
	{
		DimmerSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimmerSlot->SetOffsets(FMargin(0.0f));
	}

	// Панель по центру — двойная рамка в палитре HUD (как меню паузы/окно ежедневной награды).
	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StartFrame"));
	FrameBorder->SetPadding(FMargin(2.0f));

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StartPanel"));
	PanelBorder->SetPadding(FMargin(36.0f, 26.0f));
	FrameBorder->SetContent(PanelBorder);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StartColumn"));
	PanelBorder->SetContent(Column);

	TitleBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartTitle"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleBlock))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	SubtitleBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StartSubtitle"));
	SubtitleBlock->SetAutoWrapText(true);
	SubtitleBlock->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* SubtitleSlot = Column->AddChildToVerticalBox(SubtitleBlock))
	{
		SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
		SubtitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
	}

	if (UButton* ContinueButton = MakeMenuButton(Column, Defaults.ContinueText, TEXT("StartContinue")))
	{
		ContinueButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandleContinueClicked);
		ContinueLabel = Cast<UTextBlock>(ContinueButton->GetContent());
	}
	if (UButton* NewGameButton = MakeMenuButton(Column, Defaults.NewGameText, TEXT("StartNewGame")))
	{
		NewGameButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandleNewGameClicked);
		NewGameLabel = Cast<UTextBlock>(NewGameButton->GetContent());
	}

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(FrameBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.45f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}

	ApplyStyle(Defaults);
}

void UStartScreenWidget::ApplyStyle(const FStartScreenStyle& Style)
{
	if (DimmerBorder) { DimmerBorder->SetBrushColor(Style.DimColor); }
	if (FrameBorder)  { FrameBorder->SetBrushColor(Style.FrameColor); }
	if (PanelBorder)  { PanelBorder->SetBrushColor(Style.PanelColor); }
	if (TitleBlock)
	{
		TitleBlock->SetText(Style.TitleText);
		TitleBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
		TitleBlock->SetColorAndOpacity(FSlateColor(Style.TitleColor));
	}
	if (SubtitleBlock)
	{
		SubtitleBlock->SetText(Style.SubtitleText);
		SubtitleBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.SubtitleFontSize)));
		SubtitleBlock->SetColorAndOpacity(FSlateColor(Style.SubtitleColor));
	}

	auto StyleButtonLabel = [&Style](UTextBlock* Label, const FText& Text)
	{
		if (Label)
		{
			Label->SetText(Text);
			Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
			Label->SetColorAndOpacity(FSlateColor(Style.ButtonTextColor));
		}
	};
	StyleButtonLabel(ContinueLabel, Style.ContinueText);
	StyleButtonLabel(NewGameLabel, Style.NewGameText);

	for (USizeBox* Box : ButtonBoxes)
	{
		if (Box)
		{
			Box->SetWidthOverride(Style.ButtonSize.X);
			Box->SetHeightOverride(Style.ButtonSize.Y);
		}
	}
}

UButton* UStartScreenWidget::MakeMenuButton(UVerticalBox* Column, const FText& Label, const FName& BaseName)
{
	// SizeBox задаёт тач-габарит кнопки (у UButton 5.5 нет SetPadding): палец должен попадать.
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Box"))));
	ButtonBoxes.Add(Box);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), BaseName);
	Box->SetContent(Button);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Label"))));
	Text->SetText(Label);
	Button->SetContent(Text);

	if (UVerticalBoxSlot* BoxSlot = Column->AddChildToVerticalBox(Box))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}
	return Button;
}

void UStartScreenWidget::HandleContinueClicked()
{
	OnContinueRequested.Broadcast();
}

void UStartScreenWidget::HandleNewGameClicked()
{
	OnNewGameRequested.Broadcast();
}

// --- Модальный барьер: события мимо кнопок не идут дальше в мир ---

FReply UStartScreenWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UStartScreenWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply UStartScreenWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
	return FReply::Handled();
}

FReply UStartScreenWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
	return FReply::Handled();
}
