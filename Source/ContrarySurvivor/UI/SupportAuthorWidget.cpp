// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/SupportAuthorWidget.h"
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
#include "Styling/SlateBrush.h"

namespace SupportAuthorLocal
{
	// Требование Б8: область нажатия не меньше 48 точек по высоте. Ниже этой границы габарит
	// кнопок и крестика не опускается, даже если в настройке выставили меньше.
	static constexpr float MinTouchSizePx = 48.0f;
}

float USupportAuthorWidget::GetMinTouchSizePx()
{
	return SupportAuthorLocal::MinTouchSizePx;
}

ESlateVisibility USupportAuthorWidget::WatchAdVisibilityFor(bool bAdReady)
{
	// Дословно из задания: «Не серая, не с надписью "недоступно" — её просто нет, окно
	// остаётся с одной кнопкой». Collapsed, а не Hidden: пункт не оставляет пустого места.
	return bAdReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}

bool USupportAuthorWidget::ShouldShowWatchAdButton(bool bAdReady)
{
	// ⛔ Условие РОВНО ОДНО — готовность ролика. Порог по игровому времени (правило РИ-29,
	// AdGating::IsAdGatePassed) к этой точке НЕ применяется: игрок пришёл сюда сам, и дословно
	// из задания «Кнопка доступна всегда». Дописывать сюда проверку порога нельзя.
	return bAdReady;
}

bool USupportAuthorWidget::GrantsRewardForWatching()
{
	// ⛔ Дословно из задания: «награда превращает её в обычную ежедневку и убивает замер».
	// Ни монет, ни предметов, ни бонусов — никогда.
	return false;
}

FButtonStyle USupportAuthorWidget::MakeSupportButtonStyle(const FSupportAuthorStyle& Style)
{
	// ⛔ Признака «главная кнопка» здесь НЕТ намеренно: обе кнопки окна обязаны выглядеть
	// одинаково (условие задания), и разойтись им нечем, если стиль у них общий.
	const FLinearColor Fill = Style.ButtonFillColor;
	const float Radius = FMath::Max(0.0f, Style.ButtonCornerRadius);
	const float BorderWidth = FMath::Max(0.0f, Style.ButtonBorderWidth);

	auto Shade = [&Fill](float Scale, float TowardsWhite)
	{
		FLinearColor Result = Fill * Scale;
		Result = FMath::Lerp(Result, FLinearColor(1.0f, 1.0f, 1.0f, Fill.A), TowardsWhite);
		Result.A = Fill.A;
		return Result;
	};

	auto MakeBrush = [Radius, BorderWidth, &Style](const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Tint);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		Brush.OutlineSettings.Color = FSlateColor(Style.ButtonBorderColor);
		Brush.OutlineSettings.Width = BorderWidth;
		return Brush;
	};

	FButtonStyle Result;
	Result.Normal = MakeBrush(Fill);
	Result.Hovered = MakeBrush(Shade(1.0f, 0.10f));
	Result.Pressed = MakeBrush(Shade(0.75f, 0.0f));
	Result.Disabled = MakeBrush(FLinearColor(Fill.R, Fill.G, Fill.B, Fill.A * 0.5f));
	Result.NormalPadding = FMargin(0.0f);
	Result.PressedPadding = FMargin(0.0f);
	return Result;
}

void USupportAuthorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	// Детект как в остальных окнах проекта: дерево из готового ассета уже построено, значит
	// строить и красить нам нечего.
	bDesignerTree = (WidgetTree->RootWidget != nullptr);
	if (bDesignerTree)
	{
		struct { const UWidget* W; const TCHAR* Name; } Expected[] =
		{
			{ DimBorder, TEXT("DimBorder") },
			{ TitleText, TEXT("TitleText") }, { MessageText, TEXT("MessageText") },
			{ WatchAdButton, TEXT("WatchAdButton") }, { WatchAdText, TEXT("WatchAdText") },
			{ SupportLinkButton, TEXT("SupportLinkButton") }, { SupportLinkText, TEXT("SupportLinkText") },
			{ CloseButton, TEXT("CloseButton") }, { CloseText, TEXT("CloseText") },
			{ ThanksText, TEXT("ThanksText") },
		};
		for (const auto& Entry : Expected)
		{
			if (!Entry.W)
			{
				UE_LOG(LogQA, Warning,
					TEXT("SupportAuthorWidget: кубик %s не найден в готовом окне — элемент отключён"),
					Entry.Name);
			}
		}
	}
	else
	{
		BuildCodeTree();
	}

	// Клики — в обоих путях: кнопки могли прийти из дизайнера, но обработчики всё равно наши.
	if (WatchAdButton)
	{
		WatchAdButton->OnClicked.AddDynamic(this, &USupportAuthorWidget::HandleWatchAdClicked);
	}
	if (SupportLinkButton)
	{
		SupportLinkButton->OnClicked.AddDynamic(this, &USupportAuthorWidget::HandleSupportLinkClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &USupportAuthorWidget::HandleCloseClicked);
	}

	if (!bDesignerTree)
	{
		ApplyStyle(CachedStyle);
	}
}

void USupportAuthorWidget::BuildCodeTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SupportRoot"));
	WidgetTree->RootWidget = Root;

	// Затемнение на весь экран. Visible — ловит касания мимо окна, чтобы они не ушли в игру.
	DimBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("DimBorder"));
	if (UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(DimBorder))
	{
		DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		DimSlot->SetOffsets(FMargin(0.0f));
	}

	// Кант и подложка окна.
	FrameBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SupportFrame"));
	FrameBorder->SetPadding(FMargin(2.0f));

	PanelBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SupportPanel"));
	PanelBorder->SetPadding(FMargin(26.0f, 22.0f));
	FrameBorder->SetContent(PanelBorder);

	USizeBox* WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SupportWidthBox"));
	PanelBorder->SetContent(WidthBox);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SupportColumn"));
	WidthBox->SetContent(Column);

	// Крестик — первой строкой, прижат вправо. Габарит под палец ставит ApplyStyle.
	{
		USizeBox* CloseBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CloseButtonBox"));
		ButtonBoxes.Add(CloseBox);

		CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
		CloseBox->SetContent(CloseButton);

		CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseText"));
		CloseText->SetJustification(ETextJustify::Center);
		CloseButton->SetContent(CloseText);

		if (UVerticalBoxSlot* CloseSlot = Column->AddChildToVerticalBox(CloseBox))
		{
			CloseSlot->SetHorizontalAlignment(HAlign_Right);
			CloseSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}
	}

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	MessageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MessageText"));
	MessageText->SetJustification(ETextJustify::Center);
	MessageText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* MessageSlot = Column->AddChildToVerticalBox(MessageText))
	{
		MessageSlot->SetHorizontalAlignment(HAlign_Fill);
		MessageSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 22.0f));
	}

	// ⛔ ПОРЯДОК КНОПОК МЕНЯТЬ НЕЛЬЗЯ (условие задания): сначала просмотр рекламы, потом
	// другие способы поддержать.
	WatchAdButton = MakeWindowButton(Column, CachedStyle.WatchAdText, TEXT("WatchAdButton"));
	if (WatchAdButton)
	{
		WatchAdText = Cast<UTextBlock>(WatchAdButton->GetContent());
	}
	SupportLinkButton = MakeWindowButton(Column, CachedStyle.SupportLinkText, TEXT("SupportLinkButton"));
	if (SupportLinkButton)
	{
		SupportLinkText = Cast<UTextBlock>(SupportLinkButton->GetContent());
	}

	// Строка благодарности — под кнопками, спрятана до просмотра ролика.
	ThanksText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ThanksText"));
	ThanksText->SetJustification(ETextJustify::Center);
	ThanksText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* ThanksSlot = Column->AddChildToVerticalBox(ThanksText))
	{
		ThanksSlot->SetHorizontalAlignment(HAlign_Center);
		ThanksSlot->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	}

	// Окно по центру экрана, размер — по содержимому.
	if (UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(FrameBorder))
	{
		PanelSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		PanelSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PanelSlot->SetAutoSize(true);
		PanelSlot->SetPosition(FVector2D(0.0f, 0.0f));
	}

	// Ширину окна фиксируем здесь же: колонка внутри тянется по ней.
	WidthBox->SetWidthOverride(FMath::Max(200.0f, CachedStyle.WindowWidth));
}

UButton* USupportAuthorWidget::MakeWindowButton(UVerticalBox* Column, const FText& Label, const FName& BaseName)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Box"))));
	ButtonBoxes.Add(Box);

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), BaseName);
	Box->SetContent(Button);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
		FName(*(BaseName.ToString() + TEXT("Label"))));
	Text->SetText(Label);
	Text->SetJustification(ETextJustify::Center);
	Button->SetContent(Text);

	if (UVerticalBoxSlot* BoxSlot = Column->AddChildToVerticalBox(Box))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Center);
		BoxSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}
	return Button;
}

void USupportAuthorWidget::ApplyStyle(const FSupportAuthorStyle& Style)
{
	CachedStyle = Style;

	// Тексты ставим в ОБОИХ путях: это данные задания издателя, а не оформление.
	if (TitleText)       { TitleText->SetText(Style.TitleText); }
	if (MessageText)     { MessageText->SetText(Style.MessageText); }
	if (WatchAdText)     { WatchAdText->SetText(Style.WatchAdText); }
	if (SupportLinkText) { SupportLinkText->SetText(Style.SupportLinkText); }
	if (CloseText)       { CloseText->SetText(Style.CloseText); }
	if (ThanksText)      { ThanksText->SetText(Style.ThanksText); }

	// Дерево из дизайнера красит владелец мышкой — код туда не лезет.
	if (bDesignerTree)
	{
		return;
	}

	if (DimBorder)   { DimBorder->SetBrushColor(Style.DimColor); }
	if (FrameBorder) { FrameBorder->SetBrushColor(Style.FrameColor); }
	if (PanelBorder) { PanelBorder->SetBrushColor(Style.PanelColor); }

	// ⛔ ОДИН стиль на обе кнопки — условие задания. Крестик берёт тот же стиль: он тоже
	// кнопка окна, и своего вида у него быть не должно.
	const FButtonStyle ButtonLook = MakeSupportButtonStyle(Style);
	if (WatchAdButton)      { WatchAdButton->SetStyle(ButtonLook); }
	if (SupportLinkButton)  { SupportLinkButton->SetStyle(ButtonLook); }
	if (CloseButton)        { CloseButton->SetStyle(ButtonLook); }

	// ⛔ ОДИН габарит на обе кнопки; нижняя граница — 48 точек под палец (требование Б8).
	const float ButtonWidth = FMath::Max(SupportAuthorLocal::MinTouchSizePx, Style.ButtonSize.X);
	const float ButtonHeight = FMath::Max(SupportAuthorLocal::MinTouchSizePx, Style.ButtonSize.Y);
	for (USizeBox* Box : ButtonBoxes)
	{
		if (!Box)
		{
			continue;
		}
		// Крестик квадратный и тоже не мельче тач-границы; остальные кнопки во всю ширину.
		const bool bIsCloseBox = Box->GetContent() == CloseButton;
		Box->SetWidthOverride(bIsCloseBox ? SupportAuthorLocal::MinTouchSizePx : ButtonWidth);
		Box->SetHeightOverride(bIsCloseBox ? SupportAuthorLocal::MinTouchSizePx : ButtonHeight);
	}

	auto StyleLabel = [&Style](UTextBlock* Label, const FLinearColor& Color, int32 FontSize, bool bBold)
	{
		if (Label)
		{
			Label->SetFont(FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular",
				FMath::Max(8, FontSize)));
			Label->SetColorAndOpacity(FSlateColor(Color));
		}
	};
	StyleLabel(TitleText, Style.TitleColor, Style.TitleFontSize, /*bBold=*/true);
	StyleLabel(MessageText, Style.MessageColor, Style.MessageFontSize, false);
	StyleLabel(WatchAdText, Style.ButtonTextColor, Style.ButtonFontSize, true);
	StyleLabel(SupportLinkText, Style.ButtonTextColor, Style.ButtonFontSize, true);
	StyleLabel(CloseText, Style.ButtonTextColor, Style.ButtonFontSize, true);
	StyleLabel(ThanksText, Style.TitleColor, Style.MessageFontSize, false);
}

void USupportAuthorWidget::SetAdAvailable(bool bInAdAvailable)
{
	bAdAvailable = bInAdAvailable;
	SetRowVisibility(WatchAdButton, WatchAdVisibilityFor(ShouldShowWatchAdButton(bAdAvailable)));
}

void USupportAuthorWidget::ShowThanks()
{
	if (ThanksText)
	{
		ThanksText->SetText(CachedStyle.ThanksText);
		ThanksText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void USupportAuthorWidget::SetRowVisibility(UWidget* Widget, ESlateVisibility InVisibility)
{
	if (!Widget)
	{
		return;
	}
	// В кодовом дереве кнопка обёрнута в SizeBox — прятать надо обёртку, иначе в колонке
	// останется пустое место (ловушка скрытия, урок AmmoRow).
	UWidget* Row = Widget;
	if (USizeBox* Box = Cast<USizeBox>(Widget->GetParent()))
	{
		Row = Box;
	}
	Row->SetVisibility(InVisibility);
}

void USupportAuthorWidget::HandleWatchAdClicked()
{
	if (bAdInProgress)
	{
		return; // ролик уже идёт — второй не заводим
	}
	if (!ShouldShowWatchAdButton(bAdAvailable))
	{
		// Ролик разгрузился между показом кнопки и нажатием — честно убираем кнопку.
		SetAdAvailable(false);
		return;
	}
	OnWatchAdRequested.Broadcast();
}

void USupportAuthorWidget::HandleSupportLinkClicked()
{
	OnSupportLinkRequested.Broadcast();
}

void USupportAuthorWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast();
}

// Модальный барьер: касание мимо кнопок съедается здесь и в игру не проходит.
FReply USupportAuthorWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply USupportAuthorWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	return FReply::Handled();
}

FReply USupportAuthorWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchStarted(InGeometry, InGestureEvent);
	return FReply::Handled();
}

FReply USupportAuthorWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	Super::NativeOnTouchEnded(InGeometry, InGestureEvent);
	return FReply::Handled();
}
