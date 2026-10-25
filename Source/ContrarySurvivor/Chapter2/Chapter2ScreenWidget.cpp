// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/Chapter2ScreenWidget.h"
#include "ContrarySurvivor/Chapter2/Chapter2Component.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/Chapter2/Chapter2Settings.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h"
#include "ContrarySurvivor/UI/DialogScreenWidget.h"
#include "ContrarySurvivor/UI/TouchControlsWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Camera/PlayerCameraManager.h"
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
#include "Rendering/DrawElements.h"
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

	// Точка на окружности: угол 0 — вверх экрана, по часовой стрелке.
	FVector2D OnCircle(const FVector2D& Center, float Radius, float Angle)
	{
		return Center + FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * Radius;
	}
}

void UChapter2ScreenWidget::InitLayer(EChapter2ScreenLayer InLayer)
{
	Layer = InLayer;
	BuildTree();
}

UWidget* UChapter2ScreenWidget::GetRootForTest() const
{
	return WidgetTree ? WidgetTree->RootWidget.Get() : nullptr;
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
			GlassImage->SetRenderOpacity(FMath::Clamp(S->MaskGlassOpacity, 0.0f, 1.0f));
			GlassImage->SetVisibility(ESlateVisibility::Collapsed);
			break;
		}
		case EChapter2ScreenLayer::Bars:
		{
			UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Chapter2Bars"));
			UProgressBar* Bar = nullptr;
			UTexture2D* AirIcon = UChapter2Settings::ResolveTexture(S->AirIconTexture, UChapter2Settings::ExpectedAirIconPath);
			AirRow = MakeBarRow(WidgetTree, S->AirLabelText, AirIcon, S->BarIconSize, S->AirBarColor, S->BarWidth, TEXT("AirRow"), Bar);
			AirBar = Bar;
			Column->AddChildToVerticalBox(AirRow);
			if (UCanvasPanelSlot* ColumnSlot = Root->AddChildToCanvas(Column))
			{
				ColumnSlot->SetAnchors(FAnchors(S->BarsAnchor.X, S->BarsAnchor.Y));
				ColumnSlot->SetAlignment(FVector2D(S->BarsAnchor.X, 0.0f));
				ColumnSlot->SetPosition(S->BarsOffset);
				ColumnSlot->SetAutoSize(true);
			}
			AirRow->SetVisibility(ESlateVisibility::Collapsed);
			break;
		}
		case EChapter2ScreenLayer::Locator:
		{
			// Надпись [225] над кругом локатора; сам локатор рисует NativePaint.
			LocatorLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LocatorLabel"));
			LocatorLabel->SetText(S->DetectorLabelText);
			LocatorLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
			LocatorLabel->SetColorAndOpacity(FSlateColor(S->LocatorColor));
			LocatorLabel->SetShadowOffset(FVector2D(1.0f, 1.0f));
			LocatorLabel->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
			LocatorLabel->SetJustification(ETextJustify::Center);
			if (UCanvasPanelSlot* LabelSlot = Root->AddChildToCanvas(LocatorLabel))
			{
				LabelSlot->SetAnchors(FAnchors(0.0f, 0.0f));
				LabelSlot->SetAlignment(FVector2D(0.5f, 1.0f));
				LabelSlot->SetAutoSize(true);
			}
			LocatorLabel->SetVisibility(ESlateVisibility::Collapsed);
			// Кисти: круг с полным скруглением (рисуется одним прямоугольником), цвет — при рисовании.
			LocatorDiscBrush = FSlateRoundedBoxBrush(FLinearColor::White);
			LocatorDotBrush = FSlateRoundedBoxBrush(FLinearColor::White);
			break;
		}
		case EChapter2ScreenLayer::Story:
		{
			PhotoImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Photo"));
			if (UCanvasPanelSlot* PhotoSlot = Root->AddChildToCanvas(PhotoImage))
			{
				// Место и размер ставит TickPhoto: в свободной части экрана рядом с окном разговора.
				PhotoSlot->SetAnchors(FAnchors(0.0f, 0.0f));
				PhotoSlot->SetAlignment(FVector2D(0.0f, 0.0f));
				PhotoSlot->SetSize(S->PhotoSize);
			}
			PhotoImage->SetVisibility(ESlateVisibility::Collapsed);
			if (S->PhotoSize.Y > 1.0f)
			{
				PhotoAspect = S->PhotoSize.X / S->PhotoSize.Y;
			}

			// «Конец первой главы. Продолжение следует…» — тёмный экран с крупной надписью.
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

UTouchControlsWidget* UChapter2ScreenWidget::FindTouchLayer() const
{
	const AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetOwningPlayer());
	return PC ? PC->GetTouchControlsLayer() : nullptr;
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
			// Кнопки под стеклом ярче («Сделай так, что бы кнопки проглядывали сквозь эту маску»).
			if (UTouchControlsWidget* Touch = FindTouchLayer())
			{
				Touch->SetUnderMaskGlass(bWanted && bHasGlass);
			}
			break;
		}
		case EChapter2ScreenLayer::Bars:
		{
			const bool bModal = !PC || PC->IsAnyModalUIOpen();
			const UChapter2Component::FEnv& Env = Chapter2 ? Chapter2->GetLastEnv() : UChapter2Component::FEnv();
			const bool bAir = Chapter2 && !bModal && Chapter2->HasApparatus()
				&& (Chapter2->IsMaskOn() || Env.bInZone || Env.bNearZone);
			SetShown(AirRow, bAir);
			if (bAir && AirBar)
			{
				AirBar->SetPercent(Chapter2->GetAirFraction());
			}
			break;
		}
		case EChapter2ScreenLayer::Locator:
		{
			const bool bHidden = bMenu || !PC || PC->IsAnyModalUIOpen();
			TickLocator(MyGeometry, InDeltaTime, Chapter2, bHidden);
			break;
		}
		case EChapter2ScreenLayer::Story:
		{
			TickPhoto(MyGeometry, Chapter2, bMenu);

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

void UChapter2ScreenWidget::TickLocator(const FGeometry& MyGeometry, float InDeltaTime, const UChapter2Component* Chapter2, bool bHidden)
{
	const UChapter2Settings* S = UChapter2Settings::Get();
	UTouchControlsWidget* Touch = FindTouchLayer();
	const bool bWanted = Chapter2 && Chapter2->IsDetectorShown() && !bHidden;
	if (Touch)
	{
		// Стик крупнее и без своей подложки — её место занимает экран локатора.
		Touch->SetLocatorMode(bWanted, S->LocatorStickScale);
	}
	if (!bWanted)
	{
		bLocatorVisible = false;
		LocatorSignals.Reset();
		SetShown(LocatorLabel, false);
		return;
	}

	// Где круг: ровно на стике сенсорного слоя; стика нет — в левом нижнем углу на его месте.
	bool bPlaced = false;
	FVector2D AbsCenter;
	float AbsRadius = 0.0f;
	if (Touch && Touch->IsVisible() && Touch->GetStickCircleAbsolute(AbsCenter, AbsRadius))
	{
		LocatorCenter = MyGeometry.AbsoluteToLocal(AbsCenter);
		LocatorRadius = static_cast<float>((MyGeometry.AbsoluteToLocal(AbsCenter + FVector2D(AbsRadius, 0.0)) - LocatorCenter).Size());
		bPlaced = LocatorRadius > 4.0f;
	}
	if (!bPlaced)
	{
		const FVector2D Size = MyGeometry.GetLocalSize();
		const float Unit = static_cast<float>(Size.Y) / 1080.0f;
		LocatorCenter = FVector2D(S->LocatorFallbackOffset.X * Unit, Size.Y - S->LocatorFallbackOffset.Y * Unit);
		LocatorRadius = S->LocatorFallbackRadius * Unit;
	}

	// Направления на образцы — с учётом поворота камеры: верх локатора = верх экрана игры.
	float CameraYaw = 0.0f;
	if (const APlayerController* PC = GetOwningPlayer())
	{
		CameraYaw = PC->PlayerCameraManager ? static_cast<float>(PC->PlayerCameraManager->GetCameraRotation().Yaw)
			: static_cast<float>(PC->GetControlRotation().Yaw);
	}
	const UChapter2Component::FEnv& Env = Chapter2->GetLastEnv();
	LocatorSignals.Reset();
	NearestStrength = 0.0f;
	for (const FVector& SampleLocation : Env.SampleLocations)
	{
		const FVector2D Delta(SampleLocation.X - Env.HeroLocation.X, SampleLocation.Y - Env.HeroLocation.Y);
		const float Distance = static_cast<float>(Delta.Size());
		const float Strength = Chapter2Rules::SignalStrength(Distance, S->DetectorRange);
		if (Strength <= 0.0f)
		{
			continue; // сигнал не долетает — локатор его не ловит
		}
		FLocatorSignal Signal;
		Signal.Dir = Chapter2Rules::LocatorScreenDir(Delta, CameraYaw);
		Signal.Range01 = FMath::Clamp(Distance / FMath::Max(1.0f, S->DetectorRange), 0.0f, 1.0f);
		Signal.Strength = Strength;
		NearestStrength = FMath::Max(NearestStrength, Strength);
		LocatorSignals.Add(Signal);
	}

	// Стрелка-подсказка: через время без находки — на ближайший образец.
	bLocatorArrow = false;
	FVector Nearest;
	if (Chapter2->IsHintArrowShown() && Chapter2->GetNearestSampleLocation(Nearest))
	{
		bLocatorArrow = true;
		LocatorArrowDir = Chapter2Rules::LocatorScreenDir(
			FVector2D(Nearest.X - Env.HeroLocation.X, Nearest.Y - Env.HeroLocation.Y), CameraYaw);
	}

	LocatorTime += FMath::Max(0.0f, InDeltaTime);
	if (UCanvasPanelSlot* LabelSlot = LocatorLabel ? Cast<UCanvasPanelSlot>(LocatorLabel->Slot) : nullptr)
	{
		const FVector2D LabelPos(LocatorCenter.X, LocatorCenter.Y - LocatorRadius - 4.0f);
		if (!LabelSlot->GetPosition().Equals(LabelPos, 0.5))
		{
			LabelSlot->SetPosition(LabelPos);
		}
	}
	SetShown(LocatorLabel, true);
	bLocatorVisible = true;
}

void UChapter2ScreenWidget::TickPhoto(const FGeometry& MyGeometry, const UChapter2Component* Chapter2, bool bMenu)
{
	const UChapter2Settings* S = UChapter2Settings::Get();
	const bool bPhoto = Chapter2 && Chapter2->IsPhotoShown() && !bMenu;
	if (bPhoto && !bPhotoResolved && PhotoImage)
	{
		bPhotoResolved = true;
		if (UTexture2D* Tex = UChapter2Settings::ResolveTexture(S->PhotoTexture, UChapter2Settings::ExpectedPhotoPath))
		{
			PhotoImage->SetBrushFromTexture(Tex);
			bHasPhoto = true;
			if (Tex->GetSizeX() > 0 && Tex->GetSizeY() > 0)
			{
				PhotoAspect = static_cast<float>(Tex->GetSizeX()) / static_cast<float>(Tex->GetSizeY());
			}
		}
	}
	if (!bPhoto || !bHasPhoto || !PhotoImage)
	{
		SetShown(PhotoImage, false);
		return;
	}

	// Фотография — в свободной части экрана рядом с окном разговора (правка Рината 25.10 вечер:
	// «фотография частично перекрывает область диалога (реплики и кнопки)»), при любых
	// пропорциях экрана.
	FBox2D Occupied(ForceInit);
	const APlayerController* PC = GetOwningPlayer();
	const AContrarySurvivorHUD* CSHUD = PC ? PC->GetHUD<AContrarySurvivorHUD>() : nullptr;
	if (const UDialogScreenWidget* Dialog = CSHUD ? CSHUD->GetDialogWidgetOnScreen() : nullptr)
	{
		const FBox2D Abs = Dialog->GetOccupiedAbsoluteBox();
		if (Abs.bIsValid)
		{
			const FVector2D LocalMin = MyGeometry.AbsoluteToLocal(Abs.Min);
			const FVector2D LocalMax = MyGeometry.AbsoluteToLocal(Abs.Max);
			Occupied += LocalMin;
			Occupied += LocalMax;
		}
	}
	const FBox2D Fit = Chapter2Rules::FitPhoto(MyGeometry.GetLocalSize(), Occupied, PhotoAspect, S->PhotoSize, S->PhotoMargin);
	if (!Fit.bIsValid)
	{
		SetShown(PhotoImage, false);
		return;
	}
	if (UCanvasPanelSlot* PhotoSlot = Cast<UCanvasPanelSlot>(PhotoImage->Slot))
	{
		if (!PhotoSlot->GetPosition().Equals(Fit.Min, 0.5) || !PhotoSlot->GetSize().Equals(Fit.GetSize(), 0.5))
		{
			PhotoSlot->SetPosition(Fit.Min);
			PhotoSlot->SetSize(Fit.GetSize());
		}
	}
	SetShown(PhotoImage, true);
}

int32 UChapter2ScreenWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);
	if (Layer != EChapter2ScreenLayer::Locator || !bLocatorVisible || LocatorRadius < 4.0f)
	{
		return MaxLayer;
	}

	const UChapter2Settings* S = UChapter2Settings::Get();
	const FVector2D C = LocatorCenter;
	const float R = LocatorRadius;
	const FLinearColor Green = S->LocatorColor;
	const FLinearColor Signal = S->LocatorSignalColor;
	int32 DrawLayer = MaxLayer + 1;
	const FPaintGeometry WholeArea = AllottedGeometry.ToPaintGeometry();

	auto WithAlpha = [](const FLinearColor& Color, float Alpha)
	{
		return FLinearColor(Color.R, Color.G, Color.B, FMath::Clamp(Alpha, 0.0f, 1.0f));
	};
	auto Circle = [&](const FVector2D& Center, float Radius, const FLinearColor& Color, float Thickness, int32 Segments)
	{
		if (Radius < 0.5f || Color.A <= 0.0f)
		{
			return;
		}
		TArray<FVector2D> Points;
		Points.Reserve(Segments + 1);
		for (int32 Index = 0; Index <= Segments; ++Index)
		{
			Points.Add(OnCircle(Center, Radius, 2.0f * PI * Index / Segments));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, WholeArea, Points, ESlateDrawEffect::None, Color, true, Thickness);
	};
	auto Line = [&](const FVector2D& From, const FVector2D& To, const FLinearColor& Color, float Thickness)
	{
		TArray<FVector2D> Points = { From, To };
		FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, WholeArea, Points, ESlateDrawEffect::None, Color, true, Thickness);
	};
	auto Dot = [&](const FVector2D& Center, float Size, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, DrawLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(Size, Size), FSlateLayoutTransform(FVector2f(Center - FVector2D(Size, Size) * 0.5))),
			&LocatorDotBrush, ESlateDrawEffect::None, Color);
	};

	// 1) «Сам стик чернел»: тёмный круг экрана локатора.
	FSlateDrawElement::MakeBox(OutDrawElements, DrawLayer,
		AllottedGeometry.ToPaintGeometry(FVector2f(2.0f * R, 2.0f * R), FSlateLayoutTransform(FVector2f(C - FVector2D(R, R)))),
		&LocatorDiscBrush, ESlateDrawEffect::None, FLinearColor(0.01f, 0.025f, 0.01f, 0.92f));
	++DrawLayer;

	// 2) Сетка: два кольца дальности и перекрестье.
	Circle(C, R * 0.33f, WithAlpha(Green, 0.22f), 1.0f, 32);
	Circle(C, R * 0.66f, WithAlpha(Green, 0.22f), 1.0f, 40);
	Line(C - FVector2D(R, 0.0), C + FVector2D(R, 0.0), WithAlpha(Green, 0.14f), 1.0f);
	Line(C - FVector2D(0.0, R), C + FVector2D(0.0, R), WithAlpha(Green, 0.14f), 1.0f);

	// 3) Зелёные волны из центра: чем сильнее ближайший сигнал, тем чаще.
	const float WavePeriod = FMath::Lerp(2.2f, 0.7f, NearestStrength);
	for (int32 Wave = 0; Wave < 3; ++Wave)
	{
		const float Phase = FMath::Frac(LocatorTime / WavePeriod + Wave / 3.0f);
		Circle(C, R * Phase, WithAlpha(Green, (1.0f - Phase) * 0.7f), 2.0f, 40);
	}

	// 4) Вращающаяся линия со следом.
	const float Sweep = FMath::Frac(LocatorTime / FMath::Max(0.3f, S->LocatorSweepSeconds)) * 2.0f * PI;
	for (int32 Trail = 7; Trail >= 0; --Trail)
	{
		const float Angle = Sweep - Trail * 0.07f;
		Line(C, OnCircle(C, R, Angle), WithAlpha(Green, 0.85f * (1.0f - Trail / 8.0f)), Trail == 0 ? 2.5f : 2.0f);
	}
	++DrawLayer;

	// 5) Сигналы образцов: точка там, откуда идёт сигнал, свои волны от неё и искажение края
	// круга в её сторону. Линия «ловит» сигнал — точка вспыхивает, когда линия по ней прошла.
	for (const FLocatorSignal& Sig : LocatorSignals)
	{
		const FVector2D Pos = C + Sig.Dir * (Sig.Range01 * R * 0.9f);
		const float SignalAngle = FMath::Atan2(static_cast<float>(Sig.Dir.X), static_cast<float>(-Sig.Dir.Y));
		float SinceSweep = Sweep - SignalAngle;
		while (SinceSweep < 0.0f)
		{
			SinceSweep += 2.0f * PI;
		}
		const float Caught = 1.0f - FMath::Clamp(SinceSweep / 1.2f, 0.0f, 1.0f);
		const float Base = 0.35f + 0.65f * Sig.Strength;

		for (int32 Ring = 0; Ring < 2; ++Ring)
		{
			const float Phase = FMath::Frac(LocatorTime * (0.6f + 1.6f * Sig.Strength) + Ring * 0.5f);
			Circle(Pos, R * (0.05f + 0.28f * Phase), WithAlpha(Signal, (1.0f - Phase) * 0.6f * Base), 1.5f, 20);
		}

		TArray<FVector2D> Arc;
		for (int32 Step = -10; Step <= 10; ++Step)
		{
			const float Angle = SignalAngle + Step * 0.035f;
			const float Wobble = 0.035f * Sig.Strength * FMath::Sin(Step * 1.7f + LocatorTime * 12.0f);
			Arc.Add(OnCircle(C, R * (0.965f + Wobble), Angle));
		}
		FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, WholeArea, Arc, ESlateDrawEffect::None,
			WithAlpha(Signal, 0.35f + 0.55f * Sig.Strength), true, 2.0f);

		Dot(Pos, 5.0f + 8.0f * Sig.Strength + 5.0f * Caught, WithAlpha(Signal, Base * (0.45f + 0.55f * Caught)));
	}

	// 6) Полупрозрачная стрелка на ближайший образец — когда долго не получается найти.
	if (bLocatorArrow)
	{
		const FVector2D Dir = LocatorArrowDir;
		const FVector2D Side(-Dir.Y, Dir.X);
		const FVector2D Tip = C + Dir * (R * 0.8f);
		const FLinearColor ArrowColor(1.0f, 1.0f, 1.0f, FMath::Clamp(S->LocatorArrowOpacity, 0.0f, 1.0f));
		Line(C - Dir * (R * 0.15f), Tip, ArrowColor, 5.0f);
		TArray<FVector2D> Head = { Tip - Dir * (R * 0.25f) + Side * (R * 0.16f), Tip, Tip - Dir * (R * 0.25f) - Side * (R * 0.16f) };
		FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer, WholeArea, Head, ESlateDrawEffect::None, ArrowColor, true, 5.0f);
	}

	// 7) Ободок локатора.
	Circle(C, R, WithAlpha(Green, 0.9f), 2.0f, 48);
	return DrawLayer;
}
