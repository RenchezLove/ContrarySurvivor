// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/DailyRewardWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"

namespace
{
	// Сплошная кисть со скруглением (стиль золотой кнопки; паттерн MakeRoundedBrush
	// редакторного генератора, но в рантайме — виджет строится кодом без ассета).
	FSlateBrush MakeRoundedRuntimeBrush(const FLinearColor& Tint)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Tint);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(4.0f, 4.0f, 4.0f, 4.0f);
		return Brush;
	}
}

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

	// --- Build 1.2: золотая кнопка «Забрать вдвое больше» ПОД обычной (ТЗ №3 раздел 3:
	// обычная кнопка ничем не блокируется и стоит первой). В дереве всегда есть, видимость
	// решает владелец через SetupDoubleOffer; по умолчанию спрятана.
	DoubleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DailyDouble"));
	DoubleButton->OnClicked.AddDynamic(this, &UDailyRewardWidget::HandleDoubleClicked);
	DoubleButton->SetVisibility(ESlateVisibility::Collapsed);

	UHorizontalBox* DoubleRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("DailyDoubleRow"));

	// Единая иконка видео rewarded-кнопок (ТЗ раздел 0 п.8); текстуры может не быть
	// (первый запуск до генерации ассета) — тогда кнопка без иконки, текст остаётся.
	if (UTexture2D* AdIconTexture = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/UI/Icons/T_Icon_AdVideo.T_Icon_AdVideo")))
	{
		UImage* AdIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DailyDoubleIcon"));
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(AdIconTexture);
		IconBrush.SetImageSize(FVector2D(24.0f, 24.0f));
		AdIcon->SetBrush(IconBrush);
		if (UHorizontalBoxSlot* IconSlot = DoubleRow->AddChildToHorizontalBox(AdIcon))
		{
			IconSlot->SetVerticalAlignment(VAlign_Center);
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		}
	}

	UVerticalBox* DoubleLabels = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("DailyDoubleLabels"));
	DoubleLabelBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DailyDoubleLabel"));
	if (UVerticalBoxSlot* DoubleLabelSlot = DoubleLabels->AddChildToVerticalBox(DoubleLabelBlock))
	{
		DoubleLabelSlot->SetHorizontalAlignment(HAlign_Center);
	}
	DoubleSubBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DailyDoubleSub"));
	if (UVerticalBoxSlot* DoubleSubSlot = DoubleLabels->AddChildToVerticalBox(DoubleSubBlock))
	{
		DoubleSubSlot->SetHorizontalAlignment(HAlign_Center);
	}
	if (UHorizontalBoxSlot* LabelsSlot = DoubleRow->AddChildToHorizontalBox(DoubleLabels))
	{
		LabelsSlot->SetVerticalAlignment(VAlign_Center);
	}
	DoubleButton->SetContent(DoubleRow);

	if (UVerticalBoxSlot* DoubleButtonSlot = Column->AddChildToVerticalBox(DoubleButton))
	{
		DoubleButtonSlot->SetHorizontalAlignment(HAlign_Center);
		DoubleButtonSlot->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));
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

	// Build 1.2: золотая кнопка удвоения — сплошной стиль всех состояний в едином золоте
	// rewarded-кнопок; подстрока с числами собирается в SetupDoubleOffer.
	if (DoubleButton)
	{
		FButtonStyle GoldStyle;
		GoldStyle.Normal = MakeRoundedRuntimeBrush(Style.DoubleButtonColor);
		GoldStyle.Hovered = MakeRoundedRuntimeBrush(Style.DoubleButtonColor * 1.15f);
		GoldStyle.Pressed = MakeRoundedRuntimeBrush(Style.DoubleButtonColor * 1.3f);
		GoldStyle.Disabled = MakeRoundedRuntimeBrush(
			FLinearColor(Style.DoubleButtonColor.R, Style.DoubleButtonColor.G,
				Style.DoubleButtonColor.B, Style.DoubleButtonColor.A * 0.5f));
		GoldStyle.NormalPadding = FMargin(14.0f, 6.0f);
		GoldStyle.PressedPadding = FMargin(14.0f, 6.0f);
		DoubleButton->SetStyle(GoldStyle);
	}
	if (DoubleLabelBlock)
	{
		DoubleLabelBlock->SetText(Style.DoubleButtonText);
		DoubleLabelBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.DoubleButtonFontSize)));
		DoubleLabelBlock->SetColorAndOpacity(FSlateColor(Style.DoubleButtonTextColor));
	}
	if (DoubleSubBlock)
	{
		DoubleSubBlock->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
		DoubleSubBlock->SetColorAndOpacity(FSlateColor(Style.DoubleButtonTextColor));
	}
}

void UDailyRewardWidget::SetupDoubleOffer(float BaseReward, bool bVisible)
{
	if (!DoubleButton)
	{
		return;
	}
	DoubleButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bVisible && DoubleSubBlock)
	{
		// Конкретные числа, не «×2» (ТЗ №3 раздел 3): «100 монет вместо 50 …».
		FFormatNamedArguments Args;
		Args.Add(TEXT("Double"), FText::AsNumber(FMath::RoundToInt32(BaseReward * 2.0f)));
		Args.Add(TEXT("Base"), FText::AsNumber(FMath::RoundToInt32(BaseReward)));
		DoubleSubBlock->SetText(FText::Format(CurrentStyle.DoubleSubFormat, Args));
	}
}

void UDailyRewardWidget::ShowDoubledResult(float TotalAmount)
{
	// Подтверждение с итоговым количеством (ТЗ №3 п.5); кнопка удвоения прячется,
	// баннер закрывается обычной «Забрать».
	if (RewardText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Amount"), FText::AsNumber(FMath::RoundToInt32(TotalAmount)));
		RewardText->SetText(FText::Format(CurrentStyle.DoubledRewardFormat, Args));
	}
	if (DoubleButton)
	{
		DoubleButton->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UDailyRewardWidget::ShowAdNotFinished()
{
	// Кнопка остаётся — повторная попытка разрешена (ТЗ №3 п.5).
	if (DoubleSubBlock)
	{
		DoubleSubBlock->SetText(CurrentStyle.AdNotFinishedText);
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
	// Обычное «Забрать» закрывает баннер сразу — никаких догоняющих окон «а не удвоить ли»
	// (прямой запрет ТЗ №3 раздел 3).
	OnClosed.Broadcast();
	RemoveFromParent();
}

void UDailyRewardWidget::HandleDoubleClicked()
{
	OnDoubleRequested.Broadcast(); // ролик и начисление — у владельца (UDailyRewardComponent)
}
