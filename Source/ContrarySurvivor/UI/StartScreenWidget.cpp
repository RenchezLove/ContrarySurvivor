// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/StartScreenWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA: предупреждения о недостающих кубиках
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

	// ТЗ Рината 08-07, детект как в EndOfStoryWidget.cpp: дерево владельца из WBP уже
	// построено и кубики привязаны — строить и стилизовать ничего не нужно.
	bDesignerTree = (WidgetTree->RootWidget != nullptr);
	if (bDesignerTree)
	{
		struct { const UWidget* W; const TCHAR* Name; } Expected[] =
		{
			{ TitleText, TEXT("TitleText") }, { SubtitleText, TEXT("SubtitleText") },
			{ ContinueButton, TEXT("ContinueButton") }, { ContinueText, TEXT("ContinueText") },
			{ NewGameButton, TEXT("NewGameButton") }, { NewGameText, TEXT("NewGameText") },
		};
		for (const auto& Entry : Expected)
		{
			if (!Entry.W)
			{
				UE_LOG(LogQA, Warning,
					TEXT("StartScreenWidget: кубик %s не найден в WBP_StartScreen — элемент отключён"),
					Entry.Name);
			}
		}
	}
	else
	{
		BuildCodeTree();
	}

	// Клики — в обоих путях (в WBP кнопки пришли из дизайнера, обработчики всё равно наши).
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandleContinueClicked);
	}
	if (NewGameButton)
	{
		NewGameButton->OnClicked.AddDynamic(this, &UStartScreenWidget::HandleNewGameClicked);
	}

	if (!bDesignerTree)
	{
		ApplyStyle(CachedStyle);
	}
}

void UStartScreenWidget::BuildCodeTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("StartRoot"));
	WidgetTree->RootWidget = Root;

	// Затемнение на весь экран. Visible — ловит хит-тест, чтобы клик мимо кнопок не ушёл в мир.
	DimBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	if (UCanvasPanelSlot* DimmerSlot = Root->AddChildToCanvas(DimBorder))
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

	const FStartScreenStyle Defaults;

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SubtitleText"));
	SubtitleText->SetAutoWrapText(true);
	SubtitleText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* SubtitleSlot = Column->AddChildToVerticalBox(SubtitleText))
	{
		SubtitleSlot->SetHorizontalAlignment(HAlign_Center);
		SubtitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
	}

	ContinueButton = MakeMenuButton(Column, Defaults.ContinueText, TEXT("ContinueButton"));
	if (ContinueButton)
	{
		ContinueText = Cast<UTextBlock>(ContinueButton->GetContent());
	}
	NewGameButton = MakeMenuButton(Column, Defaults.NewGameText, TEXT("NewGameButton"));
	if (NewGameButton)
	{
		NewGameText = Cast<UTextBlock>(NewGameButton->GetContent());
	}

	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(FrameBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.45f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetPosition(FVector2D::ZeroVector);
	}
}

void UStartScreenWidget::ApplyStyle(const FStartScreenStyle& Style)
{
	CachedStyle = Style;

	// Дерево владельца из WBP_StartScreen: цвета/шрифты/размеры — его, код не перекрашивает
	// (ТЗ Рината 08-07). Тексты переключаются ниже — они зависят от режима переспроса.
	if (!bDesignerTree)
	{
		if (DimBorder)    { DimBorder->SetBrushColor(Style.DimColor); }
		if (FrameBorder)  { FrameBorder->SetBrushColor(Style.FrameColor); }
		if (PanelBorder)  { PanelBorder->SetBrushColor(Style.PanelColor); }

		for (USizeBox* Box : ButtonBoxes)
		{
			if (Box)
			{
				Box->SetWidthOverride(Style.ButtonSize.X);
				Box->SetHeightOverride(Style.ButtonSize.Y);
			}
		}
	}

	// Новый стиль применяется в ТЕКУЩЕМ режиме (обычный выбор либо переспрос «Новая игра»):
	// иначе повторный ApplyStyle (например, из редактора) сбросил бы открытый переспрос.
	bConfirmingNewGame ? ApplyConfirmLabels(Style) : ApplyChoiceLabels(Style);
}

void UStartScreenWidget::ApplyChoiceLabels(const FStartScreenStyle& Style)
{
	if (TitleText)
	{
		TitleText->SetText(Style.TitleText);
		if (!bDesignerTree)
		{
			TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
			TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));
		}
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Style.SubtitleText);
		if (!bDesignerTree)
		{
			SubtitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.SubtitleFontSize)));
			SubtitleText->SetColorAndOpacity(FSlateColor(Style.SubtitleColor));
		}
	}

	auto StyleButtonLabel = [this, &Style](UTextBlock* Label, const FText& Text)
	{
		if (Label)
		{
			Label->SetText(Text);
			if (!bDesignerTree)
			{
				Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
				Label->SetColorAndOpacity(FSlateColor(Style.ButtonTextColor));
			}
		}
	};
	StyleButtonLabel(ContinueText, Style.ContinueText);
	StyleButtonLabel(NewGameText, Style.NewGameText);
}

void UStartScreenWidget::ApplyConfirmLabels(const FStartScreenStyle& Style)
{
	// Переспрос «Точно начать заново?» (решение лида 08-05): те же две кнопки, другие подписи —
	// «Продолжить» временно становится «Отмена», «Новая игра» — «Да, начать заново».
	if (TitleText)
	{
		TitleText->SetText(Style.ConfirmTitleText);
		if (!bDesignerTree)
		{
			TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
			TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));
		}
	}
	if (SubtitleText)
	{
		SubtitleText->SetText(Style.ConfirmSubtitleText);
		if (!bDesignerTree)
		{
			SubtitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.SubtitleFontSize)));
			SubtitleText->SetColorAndOpacity(FSlateColor(Style.SubtitleColor));
		}
	}

	auto StyleButtonLabel = [this, &Style](UTextBlock* Label, const FText& Text)
	{
		if (Label)
		{
			Label->SetText(Text);
			if (!bDesignerTree)
			{
				Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
				Label->SetColorAndOpacity(FSlateColor(Style.ButtonTextColor));
			}
		}
	};
	StyleButtonLabel(ContinueText, Style.ConfirmCancelText);
	StyleButtonLabel(NewGameText, Style.ConfirmYesText);
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
	if (bConfirmingNewGame)
	{
		// Кнопка сейчас подписана «Отмена» — переспрос закрыт, сейв цел, обычный выбор снова.
		bConfirmingNewGame = false;
		ApplyChoiceLabels(CachedStyle);
		return;
	}
	OnContinueRequested.Broadcast();
}

void UStartScreenWidget::HandleNewGameClicked()
{
	if (!bConfirmingNewGame)
	{
		// Первый клик ничего не стирает — только переспрашивает (решение лида 08-05: случайное
		// касание на телефоне не должно уничтожать прогресс без возможности отмены).
		bConfirmingNewGame = true;
		ApplyConfirmLabels(CachedStyle);
		return;
	}
	// Кнопка сейчас подписана «Да, начать заново» — второе явное нажатие стирает по-настоящему.
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
