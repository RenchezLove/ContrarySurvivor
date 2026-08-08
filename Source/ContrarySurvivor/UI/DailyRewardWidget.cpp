// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/DailyRewardWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA: предупреждения о недостающих кубиках
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

	// ТЗ Рината 08-07, детект как в EndOfStoryWidget.cpp: WBP-наследник приходит с деревом
	// владельца, построенным из ассета ДО этого вызова, — кубики уже привязаны
	// BindWidgetOptional, строить и стилизовать ничего не нужно.
	bDesignerTree = (WidgetTree->RootWidget != nullptr);
	if (bDesignerTree)
	{
		// Недостающие имена — предупреждение (элемент не работает, остальное живёт).
		struct { const UWidget* W; const TCHAR* Name; } Expected[] =
		{
			{ TitleText, TEXT("TitleText") }, { StreakText, TEXT("StreakText") },
			{ RewardText, TEXT("RewardText") },
			{ TakeButton, TEXT("TakeButton") }, { TakeText, TEXT("TakeText") },
			{ DoubleButton, TEXT("DoubleButton") },
			{ DoubleText, TEXT("DoubleText") }, { DoubleSubText, TEXT("DoubleSubText") },
		};
		for (const auto& Entry : Expected)
		{
			if (!Entry.W)
			{
				UE_LOG(LogQA, Warning,
					TEXT("DailyRewardWidget: кубик %s не найден в WBP_DailyReward — элемент отключён"),
					Entry.Name);
			}
		}
	}
	else
	{
		BuildCodeTree();
	}

	// Клики — в обоих путях (в WBP кнопки пришли из дизайнера, обработчики всё равно наши).
	if (TakeButton)
	{
		TakeButton->OnClicked.AddDynamic(this, &UDailyRewardWidget::HandleTakeClicked);
	}
	if (DoubleButton)
	{
		DoubleButton->OnClicked.AddDynamic(this, &UDailyRewardWidget::HandleDoubleClicked);
		// В ассете кнопка видима (иначе владельцу нечего редактировать в дизайнере);
		// на живом экране её показывает только SetupDoubleOffer.
		DoubleButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!bDesignerTree)
	{
		ApplyStyle(CurrentStyle);
	}
}

void UDailyRewardWidget::BuildCodeTree()
{
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

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	if (UVerticalBoxSlot* TitleSlot = Column->AddChildToVerticalBox(TitleText))
	{
		TitleSlot->SetHorizontalAlignment(HAlign_Center);
		TitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 14.0f));
	}

	StreakText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StreakText"));
	if (UVerticalBoxSlot* StreakSlot = Column->AddChildToVerticalBox(StreakText))
	{
		StreakSlot->SetHorizontalAlignment(HAlign_Center);
		StreakSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	}

	RewardText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RewardText"));
	if (UVerticalBoxSlot* RewardSlot = Column->AddChildToVerticalBox(RewardText))
	{
		RewardSlot->SetHorizontalAlignment(HAlign_Center);
		RewardSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
	}

	TakeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TakeButton"));

	TakeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TakeText"));
	TakeButton->SetContent(TakeText);

	if (UVerticalBoxSlot* ButtonSlot = Column->AddChildToVerticalBox(TakeButton))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// --- Build 1.2: золотая кнопка «Забрать вдвое больше» ПОД обычной (ТЗ №3 раздел 3:
	// обычная кнопка ничем не блокируется и стоит первой). В дереве всегда есть, видимость
	// решает владелец через SetupDoubleOffer; по умолчанию спрятана (NativeOnInitialized).
	DoubleButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DoubleButton"));

	UHorizontalBox* DoubleRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("DoubleRow"));

	// Единая иконка видео rewarded-кнопок (ТЗ раздел 0 п.8); текстуры может не быть
	// (первый запуск до генерации ассета) — тогда кнопка без иконки, текст остаётся.
	if (UTexture2D* AdIconTexture = LoadObject<UTexture2D>(nullptr,
		TEXT("/Game/UI/Icons/T_Icon_AdVideo.T_Icon_AdVideo")))
	{
		UImage* AdIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("DoubleIcon"));
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
		UVerticalBox::StaticClass(), TEXT("DoubleLabels"));
	DoubleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DoubleText"));
	if (UVerticalBoxSlot* DoubleLabelSlot = DoubleLabels->AddChildToVerticalBox(DoubleText))
	{
		DoubleLabelSlot->SetHorizontalAlignment(HAlign_Center);
	}
	DoubleSubText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DoubleSubText"));
	if (UVerticalBoxSlot* DoubleSubSlot = DoubleLabels->AddChildToVerticalBox(DoubleSubText))
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
}

void UDailyRewardWidget::ApplyStyle(const FDailyRewardStyle& Style)
{
	CurrentStyle = Style; // SetupContent берёт отсюда форматы строк

	// Дерево владельца из WBP_DailyReward: шрифты/цвета/тексты подписей — его, код не
	// перекрашивает (ТЗ Рината 08-07: «всё можно редактировать мышкой»). Форматы строк
	// с числами запомнены выше и продолжают действовать — это данные, а не вид.
	if (bDesignerTree)
	{
		return;
	}

	if (FrameBorder) { FrameBorder->SetBrushColor(Style.FrameColor); }
	if (PanelBorder) { PanelBorder->SetBrushColor(Style.PanelColor); }
	if (TitleText)
	{
		TitleText->SetText(Style.TitleText);
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TitleFontSize)));
		TitleText->SetColorAndOpacity(FSlateColor(Style.TitleColor));
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
	if (TakeText)
	{
		TakeText->SetText(Style.TakeButtonText);
		TakeText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.TakeButtonFontSize)));
		TakeText->SetColorAndOpacity(FSlateColor(Style.TakeButtonTextColor));
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
	if (DoubleText)
	{
		DoubleText->SetText(Style.DoubleButtonText);
		DoubleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.DoubleButtonFontSize)));
		DoubleText->SetColorAndOpacity(FSlateColor(Style.DoubleButtonTextColor));
	}
	if (DoubleSubText)
	{
		DoubleSubText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 11));
		DoubleSubText->SetColorAndOpacity(FSlateColor(Style.DoubleButtonTextColor));
	}
}

void UDailyRewardWidget::SetupDoubleOffer(float BaseReward, bool bVisible)
{
	if (!DoubleButton)
	{
		return;
	}
	DoubleButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bVisible && DoubleSubText)
	{
		// Конкретные числа, не «×2» (ТЗ №3 раздел 3): «100 монет вместо 50 …».
		FFormatNamedArguments Args;
		Args.Add(TEXT("Double"), FText::AsNumber(FMath::RoundToInt32(BaseReward * 2.0f)));
		Args.Add(TEXT("Base"), FText::AsNumber(FMath::RoundToInt32(BaseReward)));
		DoubleSubText->SetText(FText::Format(CurrentStyle.DoubleSubFormat, Args));
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
	if (DoubleSubText)
	{
		DoubleSubText->SetText(CurrentStyle.AdNotFinishedText);
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
