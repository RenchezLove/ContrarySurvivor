// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/Chapter2ScreenWidget.h"
#include "ContrarySurvivor/Chapter2/Chapter2Component.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

namespace
{
	// Строка «значок + подпись, под ними шкала» для слоя шкал. Значка нет (картинку ещё не
	// положили в проект) — только подпись.
	UWidget* MakeBarRow(UWidgetTree* Tree, const FText& Label, UTexture2D* Icon, float IconSize,
		const FLinearColor& FillColor, float Width, const TCHAR* Name, UProgressBar*& OutBar)
	{
		UVerticalBox* Row = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), FName(Name));

		UHorizontalBox* Header = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (Icon)
		{
			UImage* IconImage = Tree->ConstructWidget<UImage>(UImage::StaticClass());
			IconImage->SetBrushFromTexture(Icon);
			IconImage->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
			if (UHorizontalBoxSlot* IconSlot = Header->AddChildToHorizontalBox(IconImage))
			{
				IconSlot->SetVerticalAlignment(VAlign_Center);
				IconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
			}
		}

		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Text->SetText(Label);
		Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 16));
		Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f)));
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.8f));
		if (UHorizontalBoxSlot* TextSlot = Header->AddChildToHorizontalBox(Text))
		{
			TextSlot->SetVerticalAlignment(VAlign_Center);
		}
		Row->AddChildToVerticalBox(Header);

		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Width);
		Box->SetHeightOverride(14.0f);
		OutBar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		OutBar->SetFillColorAndOpacity(FillColor);
		OutBar->SetPercent(1.0f);
		Box->SetContent(OutBar);
		if (UVerticalBoxSlot* BoxSlot = Row->AddChildToVerticalBox(Box))
		{
			BoxSlot->SetPadding(FMargin(0.0f, 2.0f, 0.0f, 8.0f));
		}
		return Row;
	}
}

void UChapter2ScreenWidget::InitLayer(EChapter2ScreenLayer InLayer)
{
	Layer = InLayer;
	BuildTree();
}

void UChapter2ScreenWidget::BuildTree()
{
	if (bTreeBuilt || !WidgetTree)
	{
		return;
	}
	bTreeBuilt = true;
	const UChapter2Settings* S = UChapter2Settings::Get();

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Chapter2Root"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // нажатия проходят в игру
	WidgetTree->RootWidget = Root;

	switch (Layer)
	{
		case EChapter2ScreenLayer::MaskGlass:
		{
			GlassImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("MaskGlass"));
			if (UCanvasPanelSlot* GlassSlot = Root->AddChildToCanvas(GlassImage))
			{
				GlassSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f)); // на весь экран
				GlassSlot->SetOffsets(FMargin(0.0f));
			}
			GlassImage->SetVisibility(ESlateVisibility::Collapsed);
			break;
		}
		case EChapter2ScreenLayer::Bars:
		{
			UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Chapter2Bars"));
			UProgressBar* Bar = nullptr;
			UTexture2D* AirIcon = UChapter2Settings::ResolveTexture(S->AirIconTexture, UChapter2Settings::ExpectedAirIconPath);
			UTexture2D* DetectorIcon = UChapter2Settings::ResolveTexture(S->DetectorIconTexture, UChapter2Settings::ExpectedDetectorIconPath);
			AirRow = MakeBarRow(WidgetTree, S->AirLabelText, AirIcon, S->BarIconSize, S->AirBarColor, S->BarWidth, TEXT("AirRow"), Bar);
			AirBar = Bar;
			Column->AddChildToVerticalBox(AirRow);
			DetectorRow = MakeBarRow(WidgetTree, S->DetectorLabelText, DetectorIcon, S->BarIconSize, S->DetectorBarColor, S->BarWidth, TEXT("DetectorRow"), Bar);
			DetectorBar = Bar;
			Column->AddChildToVerticalBox(DetectorRow);
			if (UCanvasPanelSlot* ColumnSlot = Root->AddChildToCanvas(Column))
			{
				ColumnSlot->SetAnchors(FAnchors(S->BarsAnchor.X, S->BarsAnchor.Y));
				ColumnSlot->SetAlignment(FVector2D(S->BarsAnchor.X, 0.0f));
				ColumnSlot->SetPosition(S->BarsOffset);
				ColumnSlot->SetAutoSize(true);
			}
			AirRow->SetVisibility(ESlateVisibility::Collapsed);
			DetectorRow->SetVisibility(ESlateVisibility::Collapsed);
			break;
		}
		case EChapter2ScreenLayer::Story:
		{
			PhotoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Photo"));
			if (UCanvasPanelSlot* PhotoSlot = Root->AddChildToCanvas(PhotoImage))
			{
				PhotoSlot->SetAnchors(FAnchors(S->PhotoCenter.X, S->PhotoCenter.Y));
				PhotoSlot->SetAlignment(FVector2D(0.5f, 0.5f));
				PhotoSlot->SetSize(S->PhotoSize);
			}
			PhotoImage->SetVisibility(ESlateVisibility::Collapsed);

			// «Конец второй главы. Продолжение следует…» — тёмный экран с крупной надписью.
			EndCard = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ChapterEndCard"));
			EndCard->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.88f));
			EndCard->SetHorizontalAlignment(HAlign_Center);
			EndCard->SetVerticalAlignment(VAlign_Center);
			UTextBlock* EndText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChapterEndText"));
			EndText->SetText(S->ChapterEndText);
			EndText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 40));
			EndText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f)));
			EndText->SetJustification(ETextJustify::Center);
			EndText->SetAutoWrapText(true);
			EndCard->SetContent(EndText);
			if (UCanvasPanelSlot* CardSlot = Root->AddChildToCanvas(EndCard))
			{
				CardSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				CardSlot->SetOffsets(FMargin(0.0f));
			}
			EndCard->SetVisibility(ESlateVisibility::Collapsed);
			break;
		}
	}
}

UChapter2Component* UChapter2ScreenWidget::FindChapter2() const
{
	const APlayerController* PC = GetOwningPlayer();
	const APlayerCharacter* Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
	return Player ? Player->GetChapter2() : nullptr;
}

void UChapter2ScreenWidget::SetShown(UWidget* Widget, bool bShown)
{
	if (!Widget)
	{
		return;
	}
	const ESlateVisibility Wanted = bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	if (Widget->GetVisibility() != Wanted)
	{
		Widget->SetVisibility(Wanted);
	}
}

void UChapter2ScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const UChapter2Component* Chapter2 = FindChapter2();
	const AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetOwningPlayer());
	const bool bMenu = PC && PC->IsMainMenuOnScreen();
	const UChapter2Settings* S = UChapter2Settings::Get();

	switch (Layer)
	{
		case EChapter2ScreenLayer::MaskGlass:
		{
			const bool bWanted = Chapter2 && Chapter2->IsMaskOn() && !bMenu;
			if (bWanted && !bGlassResolved && GlassImage)
			{
				bGlassResolved = true;
				if (UTexture2D* Tex = UChapter2Settings::ResolveTexture(S->MaskGlassTexture, UChapter2Settings::ExpectedMaskGlassPath))
				{
					GlassImage->SetBrushFromTexture(Tex);
					bHasGlass = true;
				}
			}
			SetShown(GlassImage, bWanted && bHasGlass);
			break;
		}
		case EChapter2ScreenLayer::Bars:
		{
			const bool bModal = !PC || PC->IsAnyModalUIOpen();
			const UChapter2Component::FEnv& Env = Chapter2 ? Chapter2->GetLastEnv() : UChapter2Component::FEnv();
			const bool bAir = Chapter2 && !bModal && Chapter2->HasApparatus()
				&& (Chapter2->IsMaskOn() || Env.bInZone || Env.bNearZone);
			const bool bDetector = Chapter2 && !bModal && Chapter2->IsDetectorShown();
			SetShown(AirRow, bAir);
			SetShown(DetectorRow, bDetector);
			if (bAir && AirBar)
			{
				AirBar->SetPercent(Chapter2->GetAirFraction());
			}
			if (bDetector && DetectorBar)
			{
				DetectorBar->SetPercent(Chapter2->GetSignalStrength());
			}
			break;
		}
		case EChapter2ScreenLayer::Story:
		{
			const bool bPhoto = Chapter2 && Chapter2->IsPhotoShown() && !bMenu;
			if (bPhoto && !bPhotoResolved && PhotoImage)
			{
				bPhotoResolved = true;
				if (UTexture2D* Tex = UChapter2Settings::ResolveTexture(S->PhotoTexture, UChapter2Settings::ExpectedPhotoPath))
				{
					PhotoImage->SetBrushFromTexture(Tex);
					bHasPhoto = true;
				}
			}
			SetShown(PhotoImage, bPhoto && bHasPhoto);

			const float Remaining = Chapter2 ? Chapter2->GetChapterEndCardRemaining() : 0.0f;
			SetShown(EndCard, Remaining > 0.0f && !bMenu);
			if (EndCard && Remaining > 0.0f)
			{
				// Плавно проступает в первую секунду и гаснет в последнюю.
				const float Total = FMath::Max(1.0f, S->ChapterEndCardSeconds);
				const float Shown = Total - Remaining;
				EndCard->SetRenderOpacity(FMath::Clamp(FMath::Min(Shown, Remaining), 0.0f, 1.0f));
			}
			break;
		}
	}
}
