// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/LimpIndicatorWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void ULimpIndicatorWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("LimpRoot"));
	WidgetTree->RootWidget = Root;

	// Дефолты стиля; фактический стиль перекроет ApplyStyle (EditAnywhere на APlayerCharacter).
	const FLimpIndicatorStyle Defaults;

	// SizeBox фиксирует ширину, Border — подложка, текст переносится: высота плашки растёт за
	// текстом (слот канвы в авто-размере), развёрнутая подсказка занимает 2-3 строки без обрезки.
	WidthBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LimpWidth"));
	Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LimpPlate"));
	IndicatorText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LimpText"));
	IndicatorText->SetAutoWrapText(true);
	IndicatorText->SetJustification(ETextJustify::Left);
	Plate->SetContent(IndicatorText);
	WidthBox->SetContent(Plate);

	if (UCanvasPanelSlot* BoxSlot = Root->AddChildToCanvas(WidthBox))
	{
		BoxSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		BoxSlot->SetAutoSize(true); // высота — по содержимому; ширину держит SizeBox
	}

	ApplyStyle(Defaults);
}

void ULimpIndicatorWidget::ApplyStyle(const FLimpIndicatorStyle& Style)
{
	if (WidthBox)
	{
		WidthBox->SetWidthOverride(FMath::Max(50.0f, Style.BoxWidth));
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
	if (IndicatorText)
	{
		IndicatorText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FMath::Max(8, Style.FontSize)));
		IndicatorText->SetColorAndOpacity(FSlateColor(Style.TextColor));
	}
}

void ULimpIndicatorWidget::SetIndicatorText(const FText& Text)
{
	if (IndicatorText)
	{
		IndicatorText->SetText(Text);
	}
}

void ULimpIndicatorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetOwningPlayer());
	const APlayerCharacter* Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;

	// Видимость: игрок хромает И не открыт модальный экран (инвентарь/магазин/диалог/смерть/
	// пауза — там плашке не место, перенос поведения постоянных панелей). Интро отдельно не
	// проверяем: чёрный экран интро лежит выше (ZOrder 50), а на проявлении мира персонаж
	// «сам, хромая, идёт к деревне» — индикатор ранения там уместен.
	SetContentVisible(Player && Player->IsLimping() && !PC->IsAnyModalUIOpen());
}
