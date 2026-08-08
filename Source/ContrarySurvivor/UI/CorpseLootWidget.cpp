// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/CorpseLootWidget.h"
#include "ContrarySurvivor/UI/ItemTileWidget.h" // общая плитка предмета (Build 1.2.2, тайлы)
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Components/CorpseLootComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Retention/OnboardingComponent.h" // ADR-063: всплывашка «Подобрано: …»
#include "AMasterInventoryItem.h"
#include "ARangedWeapon.h" // ADR-063: детект огнестрела в луте трупа
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
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"

// Строковый UCorpseLootRowWidget УДАЛЁН (Build 1.2.2): обыск перешёл на общую плитку
// UItemTileWidget — сетка иконок, клик по плитке = прежняя кнопка «Забрать».

// ===========================================================================
// UCorpseLootWidget — окно обыска
// ===========================================================================

void UCorpseLootWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Дефолт класса плитки — C++-плитка с кодовым деревом: окно работает без ассетов.
	if (!TileWidgetClass)
	{
		TileWidgetClass = UItemTileWidget::StaticClass();
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

	// Build 1.2.2: одно окно обслуживает и труп, и мешок-пикап, поэтому заголовок берём у
	// самого контейнера. Пусто — остаётся собственный заголовок окна («Обыск трупа»).
	if (TitleText)
	{
		const FText ContainerTitle = InCorpse ? InCorpse->SearchTitle : FText::GetEmpty();
		TitleText->SetText(ContainerTitle.IsEmpty() ? TitleLabel : ContainerTitle);
	}

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
	if (!CorpsePtr || !WidgetTree || !TileWidgetClass)
	{
		return;
	}

	// Build 1.2.2 (Ринат: «иконки в сетке, как в сталкере или LDoE»): внутри прежнего
	// ScrollBox-кубика — сетка плиток; клик по плитке = прежняя кнопка «Забрать».
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(
		UUniformGridPanel::StaticClass());
	LootList->AddChild(Grid);

	APlayerController* PC = GetOwningPlayer();
	const int32 Columns = FMath::Max(1, TileColumns);
	int32 TileIndex = 0;

	auto AddTileToGrid = [&](UItemTileWidget* Tile)
	{
		if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(
			Tile, TileIndex / Columns, TileIndex % Columns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Top);
		}
		++TileIndex;
	};

	// Плитка денег — первой (дефолт Рината: деньги и предметы одним списком); цифра в
	// углу иконки = сумма (у денег «штука» и есть монета).
	if (CorpsePtr->GetMoney() > 0.0f)
	{
		if (UItemTileWidget* Tile = CreateWidget<UItemTileWidget>(PC, TileWidgetClass))
		{
			Tile->bMoneyTile = true;
			UTexture2D* MoneyIcon = MoneyRowIcon.IsNull() ? nullptr : MoneyRowIcon.LoadSynchronous();
			Tile->SetTileData(MoneyIcon, MoneyRowLabel, FMath::RoundToInt32(CorpsePtr->GetMoney()));
			Tile->SetTileSize(TileSize, TileIconSize);
			Tile->OnTileClicked.AddUObject(this, &UCorpseLootWidget::HandleTileTake);
			AddTileToGrid(Tile);
		}
	}

	// Предметы трупа: иконка + переводимое название + количество стака.
	for (AMasterInventoryItem* Item : CorpsePtr->GetLootItems())
	{
		if (UItemTileWidget* Tile = CreateWidget<UItemTileWidget>(PC, TileWidgetClass))
		{
			Tile->Item = Item;
			// Иконки может не быть (незнакомый ключ) — плитка живёт на подписи.
			const TSoftObjectPtr<UTexture2D> SoftIcon = Item->GetItemIcon();
			Tile->SetTileData(SoftIcon.IsNull() ? nullptr : SoftIcon.LoadSynchronous(),
				Item->GetItemDisplayText(), Item->GetStackCount());
			Tile->SetTileSize(TileSize, TileIconSize);
			Tile->OnTileClicked.AddUObject(this, &UCorpseLootWidget::HandleTileTake);
			AddTileToGrid(Tile);
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

	// ADR-063 п.2 (издатель, дословно: «любой полученный огнестрел падает в рюкзак, в боевой
	// слот игрок надевает его сам»): автонадевание с трупа убрано — в бою оно могло втихую
	// заменить хорошее оружие на худшее. Ствол остаётся в рюкзаке (тот же поток, что покупка,
	// 018b2fa); игроку — короткая всплывашка-подсказка тем же тостом, что подсказки
	// онбординга (ShowTransientHint: одноразовость не ведётся, показывается на каждый ствол).
	if (Cast<ARangedWeapon>(TakenItem))
	{
		if (UOnboardingComponent* Onboarding = Player->GetOnboarding())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Item"), TakenItem->GetItemDisplayText());
			Onboarding->ShowTransientHint(FText::Format(FirearmPickupHintFormat, Args));
		}
		UE_LOG(LogQA, Display, TEXT("QA: огнестрел '%s' с трупа лёг в РЮКЗАК (без автоэкипа, ADR-063)"),
			*TakenItem->GetItemDisplayText().ToString());
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

void UCorpseLootWidget::HandleTileTake(UItemTileWidget* Tile)
{
	if (!Tile)
	{
		return;
	}

	if (Tile->bMoneyTile)
	{
		TakeMoneyToPlayer();
	}
	else if (AMasterInventoryItem* TakenItem = Tile->Item.Get())
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
