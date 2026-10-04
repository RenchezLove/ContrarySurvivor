// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Ads/MockBannerWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

void UMockBannerWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	// Кодовое дерево (приём UMockAdWidget::BuildCodeTree): канвас на весь экран, в нём полоса.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MockBannerRoot"));
	WidgetTree->RootWidget = Root;

	Strip = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MockBannerStrip"));
	Strip->SetBrushColor(FLinearColor(0.35f, 0.35f, 0.38f, 0.92f));
	Strip->SetHorizontalAlignment(HAlign_Center);
	Strip->SetVerticalAlignment(VAlign_Center);
	Root->AddChildToCanvas(Strip);

	Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MockBannerLabel"));
	Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 14));
	Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.96f, 1.0f, 1.0f)));
	Strip->SetContent(Label);

	// Полоса только показывает место: нажатия проходят сквозь неё в экран под ней.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMockBannerWidget::Setup(FName Placement, float HeightFraction)
{
	if (Label)
	{
		Label->SetText(FText::Format(
			NSLOCTEXT("MockAd", "BannerLabel", "Здесь будет рекламный баннер ({0})"),
			FText::FromName(Placement)));
	}

	if (UCanvasPanelSlot* StripSlot = Strip ? Cast<UCanvasPanelSlot>(Strip->Slot) : nullptr)
	{
		// Якоря-растяжка по всей ширине и по доле высоты: полоса занимает ровно
		// HeightFraction высоты экрана у нижнего края.
		const float Fraction = FMath::Clamp(HeightFraction, 0.0f, 0.25f);
		StripSlot->SetAnchors(FAnchors(0.0f, 1.0f - Fraction, 1.0f, 1.0f));
		StripSlot->SetOffsets(FMargin(0.0f));
	}
}
