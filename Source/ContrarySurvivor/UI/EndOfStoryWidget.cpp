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

	// Build 1.2.1 (ТЗ Д2), детект как в TouchControlsWidget.cpp: WBP-наследник приходит с
	// деревом Рината, построенным из ассета ДО этого вызова, — кубики уже привязаны
	// BindWidgetOptional, строить и стилизовать ничего не нужно.
	bDesignerTree = (WidgetTree->RootWidget != nullptr);
	if (bDesignerTree)
	{
		// Недостающие имена — предупреждение (элемент не работает, остальное живёт).
		struct { const UWidget* W; const TCHAR* Name; } Expected[] =
		{
			{ MessageText, TEXT("MessageText") }, { StatusText, TEXT("StatusText") },
			{ WriteButton, TEXT("WriteButton") }, { PlayButton, TEXT("PlayButton") },
			{ WriteButtonText, TEXT("WriteButtonText") }, { PlayButtonText, TEXT("PlayButtonText") },
		};
		for (const auto& Entry : Expected)
		{
			if (!Entry.W)
			{
				UE_LOG(LogQA, Warning,
					TEXT("EndOfStoryWidget: кубик %s не найден в WBP_EndOfStory — элемент отключён"),
					Entry.Name);
			}
		}
	}
	else
	{
		BuildCodeTree();
	}

	// Клики — в обоих путях (в WBP кнопки пришли из дизайнера, обработчики всё равно наши).
	if (WriteButton)
	{
		WriteButton->OnClicked.AddDynamic(this, &UEndOfStoryWidget::HandleWriteClicked);
	}
	if (PlayButton)
	{
		PlayButton->OnClicked.AddDynamic(this, &UEndOfStoryWidget::HandlePlayClicked);
	}
}

void UEndOfStoryWidget::BuildCodeTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EndOfStoryRoot"));
	WidgetTree->RootWidget = Root;

	// Дефолты стиля; фактический стиль перекроет ApplyStyle (EditAnywhere на HUD).
	const FEndOfStoryStyle Defaults;

	// SizeBox фиксирует ширину, Border — подложка; внутри вертикальный стек:
	// сообщение -> строка-статус (скрыта) -> ряд кнопок. Высота растёт за текстом.
	// Имена кубиков = именам BindWidgetOptional-полей (те же, что в WBP_EndOfStory).
	WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WidthBox"));
	Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Plate"));
	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("EndOfStoryStack"));

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
	MessageText->SetAutoWrapText(true);
	MessageText->SetJustification(ETextJustify::Left);

	StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StatusText"));
	StatusText->SetAutoWrapText(true);
	StatusText->SetJustification(ETextJustify::Center);
	StatusText->SetVisibility(ESlateVisibility::Collapsed);

	UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("EndOfStoryButtons"));
	WriteButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("WriteButton"));
	PlayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayButton"));
	WriteButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("WriteButtonText"));
	PlayButtonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PlayButtonText"));
	WriteButton->SetContent(WriteButtonText);
	PlayButton->SetContent(PlayButtonText);

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
	// Дерево из WBP: стиль/раскладка целиком Рината (ТЗ Д2) — HUD-стиль не применяется
	// (тот же принцип, что у тач-слоя: «код кубики не перекрашивает»).
	if (bDesignerTree)
	{
		return;
	}
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
	AContrarySurvivorPlayerController* PC =
		Cast<AContrarySurvivorPlayerController>(GetOwningPlayer());
	const bool bShouldShow = !PC || !PC->IsAnyModalUIOpen();
	const bool bWasVisible = IsContentVisible();
	SetContentVisible(bShouldShow);

	// Плашка вернулась из-под модалки: контроллер при закрытии модального экрана ставит
	// GameOnly, а в нём кнопки Slate кликов не получают — без восстановления Game+UI плашку
	// было бы не закрыть. Повторяем режим показа (тот же, что ставит HUD при первом показе).
	if (PC && bShouldShow && !bWasVisible)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}
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
