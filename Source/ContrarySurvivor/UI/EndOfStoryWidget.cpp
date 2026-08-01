// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/EndOfStoryWidget.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "HAL/PlatformProcess.h" // FPlatformProcess::LaunchURL (кнопка «Написать мне»)
#include "Styling/CoreStyle.h"

void UEndOfStoryWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EndOfStoryRoot"));
	WidgetTree->RootWidget = Root;

	// Дефолты стиля; фактический стиль перекроет ApplyStyle (EditAnywhere на HUD).
	const FEndOfStoryStyle Defaults;

	// SizeBox фиксирует ширину, Border — подложка; внутри вертикальный стек:
	// сообщение -> строка-статус (скрыта) -> ряд кнопок. Высота растёт за текстом.
	WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EndOfStoryWidth"));
	Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EndOfStoryPlate"));
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EndOfStoryStack"));

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EndOfStoryMessage"));
	MessageText->SetAutoWrapText(true);
	MessageText->SetJustification(ETextJustify::Left);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EndOfStoryStatus"));
	StatusText->SetAutoWrapText(true);
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EndOfStoryButtons"));
	WriteButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EndOfStoryWriteBtn"));
	PlayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("EndOfStoryPlayBtn"));
	WriteButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EndOfStoryWriteLabel"));
	PlayButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EndOfStoryPlayLabel"));
	WriteButton->SetContent(WriteButtonText);
	PlayButton->SetContent(PlayButtonText);
	WriteButton->OnClicked.AddDynamic(this, &UEndOfStoryWidget::HandleWriteClicked);
	PlayButton->OnClicked.AddDynamic(this, &UEndOfStoryWidget::HandlePlayClicked);

	if (UVerticalBoxSlot* MsgSlot = Stack->AddChildToVerticalBox(MessageText))
	{
		MsgSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}
	if (UVerticalBoxSlot* StatusSlot = Stack->AddChildToVerticalBox(StatusText))
	{
		StatusSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
	}
	if (UHorizontalBoxSlot* WriteSlot = ButtonRow->AddChildToHorizontalBox(WriteButton))
	{
		WriteSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
	}
	ButtonRow->AddChildToHorizontalBox(PlayButton);
	if (UVerticalBoxSlot* RowSlot = Stack->AddChildToVerticalBox(ButtonRow))
	{
		RowSlot->SetHorizontalAlignment(HAlign_Center);
	}

	Plate->SetContent(Stack);
	WidthBox->SetContent(Plate);

	if (UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(WidthBox))
	{
		// Выравнивание по X центру якоря (якорь-доля задаётся стилем), высота — по содержимому.
		BoxSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		BoxSlot->SetAutoSize(true);
	}

	ApplyStyle(Defaults);
}

void UEndOfStoryWidget::ApplyStyle(const FEndOfStoryStyle& Style)
{
	if (WidthBox)
	{
		WidthBox->SetWidthOverride(FMath::Max(200.0f, Style.BoxWidth));
		if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(WidthBox->Slot))
		{
			BoxSlot->SetAnchors(FAnchors(Style.ScreenAnchor.X, Style.ScreenAnchor.Y));
			BoxSlot->SetPosition(Style.ScreenOffset);
		}
	}
	if (Plate)
	{
		Plate->SetBrushColor(Style.PlateColor);
		Plate->SetPadding(FMargin(Style.PlatePadding.X, Style.PlatePadding.Y));
	}
	if (MessageText)
	{
		MessageText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", FMath::Max(8, Style.FontSize)));
		MessageText->SetColorAndOpacity(FSlateColor(Style.TextColor));
	}
	if (StatusText)
	{
		StatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.FontSize - 1)));
		StatusText->SetColorAndOpacity(FSlateColor(Style.StatusColor));
	}
	for (UButton* Button : { WriteButton.Get(), PlayButton.Get() })
	{
		if (Button)
		{
			Button->SetBackgroundColor(Style.ButtonColor);
		}
	}
	for (UTextBlock* Label : { WriteButtonText.Get(), PlayButtonText.Get() })
	{
		if (Label)
		{
			Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.ButtonFontSize)));
			Label->SetColorAndOpacity(FSlateColor(Style.ButtonTextColor));
		}
	}
}

void UEndOfStoryWidget::InitContent(const FText& InMessage, const FText& InWriteButtonLabel,
	const FText& InPlayButtonLabel, const FText& InChannelPendingText, const FString& InChannelUrl)
{
	if (MessageText)
	{
		MessageText->SetText(InMessage);
	}
	if (WriteButtonText)
	{
		WriteButtonText->SetText(InWriteButtonLabel);
	}
	if (PlayButtonText)
	{
		PlayButtonText->SetText(InPlayButtonLabel);
	}
	ChannelPendingText = InChannelPendingText;
	ChannelUrl = InChannelUrl;
}

void UEndOfStoryWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Модальные экраны важнее плашки; закрылись — плашка возвращается (SelfHiding: прячется
	// корень дерева, сам виджет продолжает тикать).
	const AContrarySurvivorPlayerController* PC =
		Cast<AContrarySurvivorPlayerController>(GetOwningPlayer());
	SetContentVisible(!PC || !PC->IsAnyModalUIOpen());
}

void UEndOfStoryWidget::HandleWriteClicked()
{
	// Ссылки на канал пока нет (Ринат даст позже) — показываем строку «Канал скоро появится»
	// (решение game-lead: выбран простейший вариант). Ссылка появится — открываем браузер.
	if (ChannelUrl.IsEmpty())
	{
		if (StatusText)
		{
			StatusText->SetText(ChannelPendingText);
			StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		UE_LOG(LogQA, Display, TEXT("QA: end-of-story WRITE clicked (no channel url yet)"));
		return;
	}

	FString Error;
	FPlatformProcess::LaunchURL(*ChannelUrl, nullptr, &Error);
	UE_LOG(LogQA, Display, TEXT("QA: end-of-story WRITE clicked, url '%s'%s%s"),
		*ChannelUrl, Error.IsEmpty() ? TEXT("") : TEXT(", error: "), *Error);
}

void UEndOfStoryWidget::HandlePlayClicked()
{
	UE_LOG(LogQA, Display, TEXT("QA: end-of-story PLAY ON clicked (closing)"));
	CloseAndRestoreInput();
}

void UEndOfStoryWidget::CloseAndRestoreInput()
{
	// Игровой режим ввода возвращаем, только если игрок не успел открыть модалку —
	// у модалки свой режим (GameAndUI), трогать его нельзя.
	if (AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetOwningPlayer()))
	{
		if (!PC->IsAnyModalUIOpen())
		{
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = true;
		}
	}
	RemoveFromParent();
}
