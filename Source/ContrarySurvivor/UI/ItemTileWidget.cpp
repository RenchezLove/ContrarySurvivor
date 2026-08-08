// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/ItemTileWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Types/SlateEnums.h" // EButtonTouchMethod::PreciseTap (лёгкая прокрутка списков)

void UItemTileWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Создана без WBP (RootWidget пуст) — кодовое дерево-фолбэк: окна работают и до
	// генерации ассета (паттерн UCorpseLootRowWidget / этап F).
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackTree();
	}

	if (TileButton)
	{
		TileButton->OnClicked.AddDynamic(this, &UItemTileWidget::HandleTileClicked);
		// ТЗ Рината 08-08 (прокрутка списков «тяжёлая»): плитка целиком — кнопка, а кнопка по
		// умолчанию (DownAndUp) забирает касание себе и не отдаёт его ScrollBox, из-за чего
		// свайп пальцем по плитке не прокручивал список. PreciseTap меняет это ровно по
		// документации движка (SlateEnums.h): «внутри списка кнопка срабатывает только точным
		// тапом, а движение пальца прокручивает список». Тап-выбор (купить/использовать/
		// экипировать) при этом сохраняется. Работает и на кодовой плитке, и на WBP_ItemTile.
		TileButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
	}
	else
	{
		UE_LOG(LogQA, Warning, TEXT("ItemTile: кубик TileButton не найден — плитка не кликается"));
	}
	if (DropButton)
	{
		DropButton->OnClicked.AddDynamic(this, &UItemTileWidget::HandleDropClicked);
		// Мини-кнопка выброса — тоже PreciseTap: если палец начал свайп на ней, список всё
		// равно прокрутится, а точный тап по-прежнему выбрасывает предмет.
		DropButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
		DropButton->SetVisibility(ESlateVisibility::Collapsed); // включает SetDropVisible (рюкзак)
	}
}

void UItemTileWidget::BuildFallbackTree()
{
	// Плитка: [квадрат иконки + цифра в правом нижнем углу] над [название | цена | статус],
	// всё это — содержимое кнопки TileButton; мини-кнопка выброса поверх, в верхнем правом
	// углу. Габариты ставит SetTileSize (зовёт экран-владелец из своих настроек).
	TileSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TileSizeBox"));
	WidgetTree->RootWidget = TileSizeBox;
	TileSizeBox->SetWidthOverride(110.0f);
	// Минимум, не потолок — см. комментарий в SetTileSize (Б8). Реальный экран-владелец
	// сразу же перезадаёт оба значения своим SetTileSize, это только начальный дефолт.
	TileSizeBox->SetMinDesiredHeight(150.0f);

	UOverlay* TileOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("TileOverlay"));
	TileSizeBox->SetContent(TileOverlay);

	// Кнопка на всю плитку (основное действие — решает экран).
	TileButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TileButton"));
	if (UOverlaySlot* ButtonSlot = TileOverlay->AddChildToOverlay(TileButton))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UBorder* TilePlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TilePlate"));
	TilePlate->SetBrushColor(FLinearColor(0.15f, 0.16f, 0.2f, 1.0f)); // InvSlotColor окон
	TilePlate->SetPadding(FMargin(6.0f));
	TileButton->SetContent(TilePlate);

	UVerticalBox* TileStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TileStack"));
	TilePlate->SetContent(TileStack);

	// Зона иконки: квадрат + цифра количества в правом нижнем углу.
	UOverlay* IconZone = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("TileIconZone"));
	if (UVerticalBoxSlot* IconZoneSlot = TileStack->AddChildToVerticalBox(IconZone))
	{
		IconZoneSlot->SetHorizontalAlignment(HAlign_Center);
	}

	TileIconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TileIconBox"));
	TileIconBox->SetWidthOverride(86.0f);
	TileIconBox->SetHeightOverride(86.0f);
	if (UOverlaySlot* IconBoxSlot = IconZone->AddChildToOverlay(TileIconBox))
	{
		IconBoxSlot->SetHorizontalAlignment(HAlign_Center);
		IconBoxSlot->SetVerticalAlignment(VAlign_Center);
	}

	TileIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TileIcon"));
	TileIconBox->SetContent(TileIcon);

	TileCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TileCountText"));
	TileCountText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
	TileCountText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.3f, 1.0f)));
	TileCountText->SetShadowOffset(FVector2D(1.0f, 1.0f));
	TileCountText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
	if (UOverlaySlot* CountSlot = IconZone->AddChildToOverlay(TileCountText))
	{
		// Правый нижний угол ИКОНКИ — уточнение Рината.
		CountSlot->SetHorizontalAlignment(HAlign_Right);
		CountSlot->SetVerticalAlignment(VAlign_Bottom);
		CountSlot->SetPadding(FMargin(0.0f, 0.0f, 2.0f, 2.0f));
	}

	// Подпись-название ПОД иконкой — постоянная (решение Рината), с переносом строк.
	TileNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TileNameText"));
	TileNameText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
	TileNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f)));
	TileNameText->SetJustification(ETextJustify::Center);
	TileNameText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* NameSlot = TileStack->AddChildToVerticalBox(TileNameText))
	{
		NameSlot->SetHorizontalAlignment(HAlign_Fill);
		NameSlot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));
	}

	// Цена (магазин) — под названием; вне магазина спрятана.
	TilePriceText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TilePriceText"));
	TilePriceText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	TilePriceText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.3f, 1.0f)));
	TilePriceText->SetJustification(ETextJustify::Center);
	TilePriceText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* PriceSlot = TileStack->AddChildToVerticalBox(TilePriceText))
	{
		PriceSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	// Статус («Не хватает монет», ADR-049: одним потухшим цветом кнопки не обойтись).
	TileStatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TileStatusText"));
	TileStatusText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
	TileStatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.45f, 0.35f, 1.0f)));
	TileStatusText->SetJustification(ETextJustify::Center);
	TileStatusText->SetAutoWrapText(true);
	TileStatusText->SetVisibility(ESlateVisibility::Collapsed);
	if (UVerticalBoxSlot* StatusSlot = TileStack->AddChildToVerticalBox(TileStatusText))
	{
		StatusSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	// Мини-кнопка выброса ПОВЕРХ плитки (не внутри TileButton — клики не путаются).
	USizeBox* DropBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TileDropBox"));
	DropBox->SetWidthOverride(26.0f);
	DropBox->SetHeightOverride(26.0f);
	if (UOverlaySlot* DropSlot = TileOverlay->AddChildToOverlay(DropBox))
	{
		DropSlot->SetHorizontalAlignment(HAlign_Right);
		DropSlot->SetVerticalAlignment(VAlign_Top);
		DropSlot->SetPadding(FMargin(0.0f, 2.0f, 2.0f, 0.0f));
	}
	DropButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("DropButton"));
	DropButton->SetBackgroundColor(FLinearColor(0.45f, 0.15f, 0.12f, 1.0f));
	UTextBlock* DropLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DropLabel"));
	DropLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
	DropLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DropLabel->SetText(NSLOCTEXT("ItemTile", "DropLabel", "X"));
	DropButton->SetContent(DropLabel);
	DropBox->SetContent(DropButton);
}

void UItemTileWidget::SetTileData(UTexture2D* InIcon, const FText& InName, int32 InCount)
{
	if (TileIcon)
	{
		if (InIcon)
		{
			// Габарит — В КИСТИ (Brush.ImageSize): SetDesiredSizeOverride живёт только в
			// живом Slate (Image.cpp:122-128), а кисть надёжна в любой момент. Реальный
			// видимый размер ограничивает TileIconBox.
			FSlateBrush IconBrush;
			IconBrush.SetResourceObject(InIcon);
			IconBrush.ImageSize = FVector2D(64.0f, 64.0f);
			TileIcon->SetBrush(IconBrush);
			TileIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			TileIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (TileNameText)
	{
		TileNameText->SetText(InName);
	}

	if (TileCountText)
	{
		if (InCount > 1)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Count"), FText::AsNumber(InCount));
			TileCountText->SetText(FText::Format(CountFormat, Args));
			TileCountText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			TileCountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UItemTileWidget::SetPriceText(const FText& InPrice)
{
	if (TilePriceText)
	{
		TilePriceText->SetText(InPrice);
		TilePriceText->SetVisibility(InPrice.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UItemTileWidget::SetStatusText(const FText& InStatus)
{
	if (TileStatusText)
	{
		TileStatusText->SetText(InStatus);
		TileStatusText->SetVisibility(InStatus.IsEmpty()
			? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
}

void UItemTileWidget::SetActionEnabled(bool bEnabled)
{
	if (TileButton)
	{
		TileButton->SetIsEnabled(bEnabled);
	}
}

void UItemTileWidget::SetDropVisible(bool bVisible)
{
	if (DropButton)
	{
		DropButton->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UItemTileWidget::SetTileSize(const FVector2D& InTileSize, float InIconSize)
{
	if (TileSizeBox)
	{
		TileSizeBox->SetWidthOverride(InTileSize.X);

		// Б8 (издатель 08-05, п.1-3): высота — МИНИМУМ, а не жёсткий потолок. Жёсткий
		// HeightOverride заставлял SizeBox ОТЧИТЫВАТЬСЯ перед сеткой заданной высотой,
		// даже когда реальное содержимое (перенесённое на несколько строк название брони +
		// строка «Не хватает монет») выше него — Slate при этом контент НЕ обрезает, а
		// просто рисует поверх границы, из-за чего текст наезжал на плитку строкой ниже,
		// а сама прокрутка списка недосчитывала лишнюю высоту (нижний ряд обрезался).
		// SetMinDesiredHeight держит прежнюю компактную высоту у коротких плиток (как раньше)
		// и ДАЁТ вырасти длинным — UUniformGridPanel потом сам равняет ВСЕ ячейки сетки по
		// самой высокой (SUniformGridPanel.cpp:86-91 — ячейка равна самой большой плитке),
		// поэтому наезда на соседей больше нет ни в одном ряду.
		//
		// Жёсткий потолок надо СНЯТЬ ЯВНО, иначе правка ничего не даёт на живом экране:
		// в ассете WBP_ItemTile у этой коробки записан HeightOverride (так её генерирует
		// коммандлет, BuildItemTile), а SBox при заданном HeightOverride на минимум вообще
		// не смотрит — потолок возвращается первым (SBox.cpp:133-136). Без сброса высота
		// оставалась бы жёсткой везде, где плитка берётся из ассета, то есть в самой игре.
		TileSizeBox->ClearHeightOverride();
		TileSizeBox->SetMinDesiredHeight(InTileSize.Y);
	}
	if (TileIconBox)
	{
		TileIconBox->SetWidthOverride(InIconSize);
		TileIconBox->SetHeightOverride(InIconSize);
	}
}

void UItemTileWidget::HandleTileClicked()
{
	OnTileClicked.Broadcast(this);
}

void UItemTileWidget::HandleDropClicked()
{
	OnDropClicked.Broadcast(this);
}
