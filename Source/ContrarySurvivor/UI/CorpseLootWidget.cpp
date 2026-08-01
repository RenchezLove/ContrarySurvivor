// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/CorpseLootWidget.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "AMasterInventoryItem.h"
#include "UInventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

// ===========================================================================
// UCorpseLootRowWidget — одна строка списка обыска
// ===========================================================================

void UCorpseLootRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Создан без WBP (RootWidget пуст) — строим кодовое дерево-фолбэк, чтобы окно
	// работало и до генерации ассета (паттерн этапа F / UEndOfStoryWidget).
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackTree();
	}

	if (TakeButton)
	{
		TakeButton->OnClicked.AddDynamic(this, &UCorpseLootRowWidget::HandleTakeClicked);
	}
	else
	{
		UE_LOG(LogQA, Warning, TEXT("CorpseLootRow: кубик TakeButton не найден — строку нельзя забрать кликом"));
	}
}

void UCorpseLootRowWidget::BuildFallbackTree()
{
	// [иконка 32px][название (растяжка)][x3][кнопка «Забрать»] на тёмной подложке.
	USizeBox* RowBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CorpseRowBox"));
	WidgetTree->RootWidget = RowBox;
	RowBox->SetMinDesiredHeight(44.0f);

	UBorder* RowPlate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CorpseRowPlate"));
	RowPlate->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.06f));
	RowPlate->SetPadding(FMargin(8.0f, 4.0f));
	RowBox->SetContent(RowPlate);

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CorpseRowStack"));
	RowPlate->SetContent(Row);

	RowIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RowIcon"));
	if (UHorizontalBoxSlot* IconSlot = Row->AddChildToHorizontalBox(RowIcon))
	{
		IconSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
		IconSlot->SetVerticalAlignment(VAlign_Center);
	}

	RowNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RowNameText"));
	RowNameText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 14));
	RowNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f)));
	if (UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(RowNameText))
	{
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		NameSlot->SetVerticalAlignment(VAlign_Center);
	}

	RowCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RowCountText"));
	RowCountText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
	RowCountText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.3f, 1.0f)));
	if (UHorizontalBoxSlot* CountSlot = Row->AddChildToHorizontalBox(RowCountText))
	{
		CountSlot->SetPadding(FMargin(8.0f, 0.0f));
		CountSlot->SetVerticalAlignment(VAlign_Center);
	}

	// Тач-габарит кнопки задаёт SizeBox (у UButton 5.5 нет SetPadding).
	USizeBox* TakeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CorpseRowTakeBox"));
	TakeBox->SetWidthOverride(110.0f);
	TakeBox->SetHeightOverride(36.0f);
	if (UHorizontalBoxSlot* BtnSlot = Row->AddChildToHorizontalBox(TakeBox))
	{
		BtnSlot->SetVerticalAlignment(VAlign_Center);
	}

	TakeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TakeButton"));
	TakeText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TakeText"));
	TakeText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
	TakeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f)));
	TakeButton->SetContent(TakeText);
	TakeBox->SetContent(TakeButton);
}

void UCorpseLootRowWidget::SetupRow(UTexture2D* InIcon, const FText& InName, int32 InCount,
	const FText& InTakeCaption)
{
	if (RowIcon)
	{
		if (InIcon)
		{
			// Габарит фиксируем В КИСТИ (Brush.ImageSize, 32px как иконки панели статов):
			// SetDesiredSizeOverride пишет только в живой Slate и до конструирования виджета
			// теряется (Image.cpp:122-128), а кисть надёжна в любой момент.
			FSlateBrush IconBrush;
			IconBrush.SetResourceObject(InIcon);
			IconBrush.ImageSize = FVector2D(32.0f, 32.0f);
			RowIcon->SetBrush(IconBrush);
			RowIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			RowIcon->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (RowNameText)
	{
		RowNameText->SetText(InName);
	}

	if (RowCountText)
	{
		if (InCount > 1)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Count"), FText::AsNumber(InCount));
			RowCountText->SetText(FText::Format(CountFormat, Args));
			RowCountText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
		else
		{
			RowCountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (TakeText)
	{
		TakeText->SetText(InTakeCaption);
	}
}

void UCorpseLootRowWidget::HandleTakeClicked()
{
	OnTakeClicked.Broadcast(this);
}

// ===========================================================================
// UCorpseLootWidget — окно обыска
// ===========================================================================

void UCorpseLootWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Дефолт класса строки — C++-строка с кодовым деревом: окно работает без ассетов.
	if (!RowWidgetClass)
	{
		RowWidgetClass = UCorpseLootRowWidget::StaticClass();
	}

	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildFallbackTree();
	}

	if (TitleText)
	{
		TitleText->SetText(TitleLabel);
	}
	if (TakeAllText)
	{
		TakeAllText->SetText(TakeAllCaption);
	}
	if (CloseText)
	{
		CloseText->SetText(CloseCaption);
	}

	if (TakeAllButton)
	{
		TakeAllButton->OnClicked.AddDynamic(this, &UCorpseLootWidget::HandleTakeAllClicked);
	}
	else
	{
		UE_LOG(LogQA, Warning, TEXT("CorpseLootWidget: кубик TakeAllButton не найден в WBP_CorpseLoot"));
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UCorpseLootWidget::HandleCloseClicked);
	}
	else
	{
		UE_LOG(LogQA, Warning, TEXT("CorpseLootWidget: кубик CloseButton не найден в WBP_CorpseLoot"));
	}
}

void UCorpseLootWidget::BuildFallbackTree()
{
	// Центр экрана: колонка «заголовок + крестик / список / Забрать всё» на тёмной панели.
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CorpseLootRoot"));
	WidgetTree->RootWidget = Root;

	USizeBox* Frame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CorpseLootFrame"));
	Frame->SetWidthOverride(560.0f);
	Frame->SetHeightOverride(520.0f);
	if (UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(Frame))
	{
		FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FrameSlot->SetAutoSize(true);
	}

	UBorder* Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CorpseLootPlate"));
	Plate->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.92f));
	Plate->SetPadding(FMargin(16.0f, 12.0f));
	Frame->SetContent(Plate);

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CorpseLootColumn"));
	Plate->SetContent(Column);

	// Шапка: заголовок слева, крестик справа.
	UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CorpseLootHeader"));
	if (UVerticalBoxSlot* HeaderSlot = Column->AddChildToVerticalBox(Header))
	{
		HeaderSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleText"));
	TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 18));
	TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.95f, 0.95f, 1.0f)));
	if (UHorizontalBoxSlot* TitleSlot = Header->AddChildToHorizontalBox(TitleText))
	{
		TitleSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		TitleSlot->SetVerticalAlignment(VAlign_Center);
	}

	USizeBox* CloseBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CorpseLootCloseBox"));
	CloseBox->SetWidthOverride(40.0f);
	CloseBox->SetHeightOverride(36.0f);
	Header->AddChildToHorizontalBox(CloseBox);

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	CloseText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseText"));
	CloseText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
	CloseText->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f)));
	CloseButton->SetContent(CloseText);
	CloseBox->SetContent(CloseButton);

	// Список лута (деньги + предметы одним списком): растяжка на всю середину окна.
	LootList = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("LootList"));
	if (UVerticalBoxSlot* ListSlot = Column->AddChildToVerticalBox(LootList))
	{
		ListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		ListSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	}

	// Низ: кнопка «Забрать всё» по центру.
	USizeBox* TakeAllBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CorpseLootTakeAllBox"));
	TakeAllBox->SetWidthOverride(220.0f);
	TakeAllBox->SetHeightOverride(44.0f);
	if (UVerticalBoxSlot* TakeAllSlot = Column->AddChildToVerticalBox(TakeAllBox))
	{
		TakeAllSlot->SetHorizontalAlignment(HAlign_Center);
	}

	TakeAllButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("TakeAllButton"));
	TakeAllText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TakeAllText"));
	TakeAllText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 14));
	TakeAllText->SetColorAndOpacity(FSlateColor(FLinearColor(0.05f, 0.05f, 0.05f, 1.0f)));
	TakeAllButton->SetContent(TakeAllText);
	TakeAllBox->SetContent(TakeAllButton);
}

void UCorpseLootWidget::InitCorpseLoot(UCorpseLootComponent* InCorpse, APlayerCharacter* InPlayer)
{
	Corpse = InCorpse;
	Player = InPlayer;
	bCloseRequested = false;
	RefreshList();
}

void UCorpseLootWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Таймер трупа истёк под открытым окном — просим контроллер закрыть нас (один раз).
	if (!bCloseRequested && !Corpse.IsValid())
	{
		bCloseRequested = true;
		UE_LOG(LogQA, Display, TEXT("QA: CORPSE window - corpse expired, requesting close"));
		OnCloseRequested.Broadcast();
	}
}

void UCorpseLootWidget::RefreshList()
{
	if (!LootList)
	{
		UE_LOG(LogQA, Warning, TEXT("CorpseLootWidget: кубик LootList не найден — список не построен"));
		return;
	}

	LootList->ClearChildren();

	UCorpseLootComponent* CorpsePtr = Corpse.Get();
	if (!CorpsePtr)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();

	// Строка денег — первой (дефолт Рината: деньги и предметы одним списком).
	if (CorpsePtr->GetMoney() > 0.0f)
	{
		if (UCorpseLootRowWidget* Row = CreateWidget<UCorpseLootRowWidget>(PC, RowWidgetClass))
		{
			Row->bMoneyRow = true;
			Row->SetupRow(nullptr, MoneyRowLabel,
				FMath::RoundToInt32(CorpsePtr->GetMoney()), TakeCaption);
			Row->OnTakeClicked.AddUObject(this, &UCorpseLootWidget::HandleRowTake);
			LootList->AddChild(Row);
		}
	}

	// Предметы трупа: иконка + переводимое название + количество стака.
	for (AMasterInventoryItem* Item : CorpsePtr->GetLootItems())
	{
		if (UCorpseLootRowWidget* Row = CreateWidget<UCorpseLootRowWidget>(PC, RowWidgetClass))
		{
			Row->Item = Item;
			// Иконки может ещё не быть (рисует художник) — строка живёт на тексте.
			UTexture2D* Icon = Item->ItemIcon.IsNull() ? nullptr : Item->ItemIcon.LoadSynchronous();
			Row->SetupRow(Icon, Item->GetItemDisplayText(), Item->GetStackCount(), TakeCaption);
			Row->OnTakeClicked.AddUObject(this, &UCorpseLootWidget::HandleRowTake);
			LootList->AddChild(Row);
		}
	}

	// Всё забрано — заглушка «Пусто» (труп лежит до таймера, окно не закрываем насильно).
	if (!CorpsePtr->HasLoot())
	{
		if (UTextBlock* Empty = WidgetTree
			? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()) : nullptr)
		{
			Empty->SetText(EmptyLabel);
			Empty->SetFont(FCoreStyle::GetDefaultFontStyle("Italic", 14));
			Empty->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.0f)));
			Empty->SetJustification(ETextJustify::Center);
			LootList->AddChild(Empty);
		}
	}
}

bool UCorpseLootWidget::TakeItemToBackpack(AMasterInventoryItem* TakenItem)
{
	UCorpseLootComponent* CorpsePtr = Corpse.Get();
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!CorpsePtr || !Inventory || !IsValid(TakenItem))
	{
		return false;
	}

	if (!CorpsePtr->TakeItem(TakenItem))
	{
		return false; // предмета уже нет в трупе (двойной клик по устаревшей строке)
	}

	if (!Inventory->AddItem(TakenItem))
	{
		// В рюкзак не лёг (сегодня AddItem false только на null) — не оставляем сироту.
		TakenItem->Destroy();
		UE_LOG(LogQA, Warning, TEXT("CorpseLootWidget: предмет не лёг в рюкзак — уничтожен, чтобы не висел в мире"));
		return false;
	}
	return true;
}

void UCorpseLootWidget::TakeMoneyToPlayer()
{
	UCorpseLootComponent* CorpsePtr = Corpse.Get();
	UStatsComponent* Stats = Player ? Player->FindComponentByClass<UStatsComponent>() : nullptr;
	if (!CorpsePtr || !Stats)
	{
		return;
	}

	const float Taken = CorpsePtr->TakeMoney();
	if (Taken > 0.0f)
	{
		Stats->AddMoney(Taken);
	}
}

void UCorpseLootWidget::HandleRowTake(UCorpseLootRowWidget* Row)
{
	if (!Row)
	{
		return;
	}

	if (Row->bMoneyRow)
	{
		TakeMoneyToPlayer();
	}
	else if (AMasterInventoryItem* TakenItem = Row->Item.Get())
	{
		TakeItemToBackpack(TakenItem);
	}

	// Частичный обыск: остаток остаётся в трупе, список пересобирается по факту.
	RefreshList();
}

void UCorpseLootWidget::HandleTakeAllClicked()
{
	TakeMoneyToPlayer();

	if (UCorpseLootComponent* CorpsePtr = Corpse.Get())
	{
		for (AMasterInventoryItem* Item : CorpsePtr->GetLootItems())
		{
			TakeItemToBackpack(Item);
		}
	}

	RefreshList();
}

void UCorpseLootWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast(); // мир закрывает контроллер (CloseCorpseLoot)
}
