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

	// ADR-076 п.2: строка-перечень обыскиваемых. В кодовом дереве создана BuildFallbackTree;
	// в дизайнерском без кубика — создаётся на корневой канве над окном (ассет не трогаем).
	CreateSearchObjectsLineIfMissing();

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

	// ADR-076 п.2: перечень обыскиваемых («Труп волка; Труп волка; Мешок») — строкой под
	// шапкой, над списком. Текст ставит UpdateSearchObjectsLine.
	SearchObjectsListText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SearchObjectsListText"));
	SearchObjectsListText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", SearchObjectsFontSize));
	SearchObjectsListText->SetColorAndOpacity(FSlateColor(SearchObjectsColor));
	SearchObjectsListText->SetAutoWrapText(true);
	if (UVerticalBoxSlot* ObjectsSlot = Column->AddChildToVerticalBox(SearchObjectsListText))
	{
		ObjectsSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
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
	// Одиночный контейнер (мешок-пикап, старые вызовы и тесты) — частный случай группы.
	TArray<UCorpseLootComponent*> One;
	if (InCorpse)
	{
		One.Add(InCorpse);
	}
	InitCorpseLootGroup(One, InPlayer);
}

void UCorpseLootWidget::InitCorpseLootGroup(const TArray<UCorpseLootComponent*>& InCorpses,
	APlayerCharacter* InPlayer)
{
	Corpses.Reset();
	for (UCorpseLootComponent* Member : InCorpses)
	{
		if (IsValid(Member))
		{
			Corpses.Add(Member);
		}
	}
	Player = InPlayer;
	bCloseRequested = false;

	// Build 1.2.2: одно окно обслуживает и труп, и мешок-пикап, поэтому заголовок берём у
	// самого контейнера — у ПЕРВОГО, то есть у подсвеченного объекта. Пусто — остаётся
	// собственный универсальный заголовок окна («Обыск», ADR-076 п.2).
	if (TitleText)
	{
		const UCorpseLootComponent* Anchor = Corpses.Num() > 0 ? Corpses[0].Get() : nullptr;
		const FText ContainerTitle = Anchor ? Anchor->SearchTitle : FText::GetEmpty();
		TitleText->SetText(ContainerTitle.IsEmpty() ? TitleLabel : ContainerTitle);
	}

	// ADR-076 п.2: перечень обыскиваемых через точку с запятой.
	UpdateSearchObjectsLine();

	RefreshList();
}

FText UCorpseLootWidget::BuildSearchObjectsLine(const TArray<UCorpseLootComponent*>& InCorpses,
	const FText& Separator, const FText& FallbackName)
{
	// Чистая сборка (ADR-076 п.2): имя каждого контейнера (SearchObjectName), пустое —
	// запасное; всё через разделитель. Пустая группа — пустой текст (строка скрывается).
	TArray<FText> Parts;
	for (const UCorpseLootComponent* Container : InCorpses)
	{
		if (Container)
		{
			Parts.Add(Container->SearchObjectName.IsEmpty() ? FallbackName : Container->SearchObjectName);
		}
	}
	return Parts.Num() > 0 ? FText::Join(Separator, Parts) : FText::GetEmpty();
}

void UCorpseLootWidget::CreateSearchObjectsLineIfMissing()
{
	if (SearchObjectsListText || !bShowSearchObjectsList)
	{
		return;
	}
	// Дизайнер-дерево без кубика: строка над окном, верх-центр корневой канвы (ADR-076 п.2
	// «сверху в окне ИЛИ НАД НИМ»; внутрь чужой раскладки не лезем — ассет Рината).
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree ? WidgetTree->RootWidget : nullptr);
	if (!RootCanvas)
	{
		UE_LOG(LogQA, Warning,
			TEXT("CorpseLootWidget: корень WBP_CorpseLoot не канва — перечень обыскиваемых не создан"));
		return;
	}
	SearchObjectsListText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SearchObjectsListText"));
	SearchObjectsListText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", SearchObjectsFontSize));
	SearchObjectsListText->SetColorAndOpacity(FSlateColor(SearchObjectsColor));
	SearchObjectsListText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UCanvasPanelSlot* LineSlot = RootCanvas->AddChildToCanvas(SearchObjectsListText))
	{
		LineSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
		LineSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		LineSlot->SetAutoSize(true);
		LineSlot->SetPosition(FVector2D(0.0f, SearchObjectsTopOffset));
	}
}

void UCorpseLootWidget::UpdateSearchObjectsLine()
{
	if (!SearchObjectsListText)
	{
		return;
	}
	if (!bShowSearchObjectsList)
	{
		SearchObjectsListText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const FText Line = BuildSearchObjectsLine(GetGroupCorpses(), SearchObjectsSeparator, SearchObjectsFallbackName);
	SearchObjectsListText->SetText(Line);
	SearchObjectsListText->SetVisibility(Line.IsEmpty()
		? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

TArray<UCorpseLootComponent*> UCorpseLootWidget::GetGroupCorpses() const
{
	TArray<UCorpseLootComponent*> Live;
	for (const TWeakObjectPtr<UCorpseLootComponent>& Ptr : Corpses)
	{
		if (UCorpseLootComponent* Member = Ptr.Get())
		{
			Live.Add(Member);
		}
	}
	return Live;
}

float UCorpseLootWidget::GetGroupMoney() const
{
	float Total = 0.0f;
	for (const UCorpseLootComponent* Member : GetGroupCorpses())
	{
		Total += Member->GetMoney();
	}
	return Total;
}

TArray<AMasterInventoryItem*> UCorpseLootWidget::GetGroupItems() const
{
	TArray<AMasterInventoryItem*> All;
	for (const UCorpseLootComponent* Member : GetGroupCorpses())
	{
		All.Append(Member->GetLootItems());
	}
	return All;
}

bool UCorpseLootWidget::GroupHasLoot() const
{
	for (const UCorpseLootComponent* Member : GetGroupCorpses())
	{
		if (Member->HasLoot())
		{
			return true;
		}
	}
	return false;
}

UCorpseLootComponent* UCorpseLootWidget::FindItemHolder(const AMasterInventoryItem* Item) const
{
	if (!IsValid(Item))
	{
		return nullptr;
	}
	for (UCorpseLootComponent* Member : GetGroupCorpses())
	{
		if (Member->GetLootItems().Contains(Item))
		{
			return Member;
		}
	}
	return nullptr;
}

void UCorpseLootWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Все тела группы исчезли под открытым окном (истёк таймер / обысканное тело ушло в
	// землю) — просим контроллер закрыть нас (один раз). Пока живо хоть одно тело, окно
	// висит: игрок мог обыскать не всю кучу.
	if (!bCloseRequested && GetGroupCorpses().Num() == 0)
	{
		bCloseRequested = true;
		UE_LOG(LogQA, Display, TEXT("QA: CORPSE window - all corpses gone, requesting close"));
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

	if (GetGroupCorpses().Num() == 0 || !WidgetTree || !TileWidgetClass)
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
	// углу иконки = сумма (у денег «штука» и есть монета). При групповом обыске (издатель
	// п.3.1) это деньги ВСЕЙ группы одной плиткой — содержимое лежит вперемешку.
	const float GroupMoney = GetGroupMoney();
	if (GroupMoney > 0.0f)
	{
		if (UItemTileWidget* Tile = CreateWidget<UItemTileWidget>(PC, TileWidgetClass))
		{
			Tile->bMoneyTile = true;
			UTexture2D* MoneyIcon = MoneyRowIcon.IsNull() ? nullptr : MoneyRowIcon.LoadSynchronous();
			Tile->SetTileData(MoneyIcon, MoneyRowLabel, FMath::RoundToInt32(GroupMoney));
			Tile->SetTileSize(TileSize, TileIconSize);
			Tile->OnTileClicked.AddUObject(this, &UCorpseLootWidget::HandleTileTake);
			AddTileToGrid(Tile);
		}
	}

	// Предметы всех тел группы: иконка + переводимое название + количество стака.
	for (AMasterInventoryItem* Item : GetGroupItems())
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

	// Вся группа пуста — заглушка «Пусто» (тела ещё лежат: обысканное уходит в землю не
	// мгновенно, п.3.2 — окно не закрываем насильно).
	if (!GroupHasLoot())
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

bool UCorpseLootWidget::CanBackpackAcceptItem(const AMasterInventoryItem* Item) const
{
	// ВМЕСТИМОСТИ у рюкзака сегодня НЕТ: UInventoryComponent::AddItem отказывает только на
	// пустом указателе. Придумывать её здесь ЗАПРЕЩЕНО (решение game-lead), поэтому проверка
	// честно отражает сегодняшнее правило — и остаётся единственным местом, куда встанет
	// настоящая вместимость, когда её заведут. Отказ здесь = тело НЕ трогаем (п.3.4).
	return IsValid(Item) && Player && Player->GetInventory() != nullptr;
}

bool UCorpseLootWidget::TakeItemToBackpack(AMasterInventoryItem* TakenItem,
	FCorpseLootTakenSummary* OutSummary)
{
	UCorpseLootComponent* Holder = FindItemHolder(TakenItem);
	if (!Holder || !IsValid(TakenItem))
	{
		return false; // предмета уже нет в телах группы (двойной клик по устаревшей плитке)
	}

	// п.3.4: сперва спрашиваем рюкзак и только потом вынимаем из тела. Не влезло — тело
	// остаётся полным, в землю не уходит, а игрок услышит об этом словами.
	if (!CanBackpackAcceptItem(TakenItem))
	{
		if (OutSummary)
		{
			OutSummary->bBackpackRefused = true;
		}
		UE_LOG(LogQA, Display, TEXT("QA: CORPSE рюкзак не принял '%s' — тело осталось полным"),
			*TakenItem->GetItemDisplayText().ToString());
		return false;
	}

	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!Inventory)
	{
		return false; // сюда не попасть: рюкзак проверен выше — страховка от правки гейта
	}

	// Название и количество снимаем ДО передачи: при слиянии стаков рюкзак уничтожает
	// влившийся целиком актор (UInventoryComponent::AddItem), после этого читать нечего.
	const FText TakenName = TakenItem->GetItemDisplayText();
	const int32 TakenCount = FMath::Max(1, TakenItem->GetStackCount());
	const bool bFirearm = Cast<ARangedWeapon>(TakenItem) != nullptr;

	if (!Holder->TakeItem(TakenItem))
	{
		return false;
	}

	if (!Inventory->AddItem(TakenItem))
	{
		// В рюкзак не лёг (сегодня AddItem false только на null) — не оставляем сироту.
		TakenItem->Destroy();
		if (OutSummary)
		{
			OutSummary->bBackpackRefused = true;
		}
		UE_LOG(LogQA, Warning, TEXT("CorpseLootWidget: предмет не лёг в рюкзак — уничтожен, чтобы не висел в мире"));
		return false;
	}

	if (OutSummary)
	{
		OutSummary->AddItem(TakenName, TakenCount);
	}

	// ADR-063 п.2 (издатель, дословно: «любой полученный огнестрел падает в рюкзак, в боевой
	// слот игрок надевает его сам»): автонадевание с трупа убрано — в бою оно могло втихую
	// заменить хорошее оружие на худшее. Ствол остаётся в рюкзаке (тот же поток, что покупка,
	// 018b2fa); игроку — короткая всплывашка-подсказка тем же тостом, что подсказки
	// онбординга (ShowTransientHint: одноразовость не ведётся, показывается на каждый ствол).
	// При ГРУППОВОМ обыске (OutSummary задан) отдельной всплывашки нет: напоминание уедет
	// хвостом общей строки — «четыре сообщения подряд» запрещены заданием (п.3.3).
	if (bFirearm)
	{
		if (OutSummary)
		{
			OutSummary->bFirearmTaken = true;
		}
		else if (UOnboardingComponent* Onboarding = Player->GetOnboarding())
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Item"), TakenName);
			Onboarding->ShowTransientHint(FText::Format(FirearmPickupHintFormat, Args));
		}
		UE_LOG(LogQA, Display, TEXT("QA: огнестрел '%s' с трупа лёг в РЮКЗАК (без автоэкипа, ADR-063)"),
			*TakenName.ToString());
	}
	return true;
}

float UCorpseLootWidget::TakeMoneyToPlayer()
{
	UStatsComponent* Stats = Player ? Player->FindComponentByClass<UStatsComponent>() : nullptr;
	if (!Stats)
	{
		return 0.0f;
	}

	// Деньги забираются со ВСЕХ тел группы разом (п.3.1): в окне они и лежат одной плиткой.
	float Total = 0.0f;
	for (UCorpseLootComponent* Member : GetGroupCorpses())
	{
		Total += Member->TakeMoney();
	}
	if (Total > 0.0f)
	{
		Stats->AddMoney(Total);
	}
	return Total;
}

void FCorpseLootTakenSummary::AddItem(const FText& Name, int32 Count)
{
	if (Count <= 0)
	{
		return;
	}
	// Одинаковые названия складываются в одну запись: «4 шкуры волка», а не четыре строки.
	for (TPair<FText, int32>& Entry : Items)
	{
		if (Entry.Key.EqualTo(Name))
		{
			Entry.Value += Count;
			return;
		}
	}
	Items.Emplace(Name, Count);
}

FText UCorpseLootWidget::PickMoneyWord(int32 Amount, const FText& One, const FText& Few, const FText& Many)
{
	// Русский счёт: 1 монета, 2-4 монеты, 5 и больше — монет; 11-14 всегда «монет».
	const int32 Abs = FMath::Abs(Amount);
	const int32 LastTwo = Abs % 100;
	if (LastTwo >= 11 && LastTwo <= 14)
	{
		return Many;
	}
	switch (Abs % 10)
	{
		case 1:           return One;
		case 2: case 3: case 4: return Few;
		default:          return Many;
	}
}

FText UCorpseLootWidget::BuildTakenSummaryText(const FCorpseLootTakenSummary& Summary) const
{
	// п.3.3: ОДНА строка на весь групповой обыск («Получено: Шкура волка x4, 12 монет»),
	// а не отдельное сообщение на каждый предмет.
	if (Summary.IsEmpty())
	{
		return Summary.bBackpackRefused ? BackpackFullHint : FText::GetEmpty();
	}

	const FString Separator = TakenSummarySeparator.ToString();
	FString List;

	for (const TPair<FText, int32>& Entry : Summary.Items)
	{
		if (!List.IsEmpty())
		{
			List += Separator;
		}
		if (Entry.Value > 1)
		{
			FFormatNamedArguments ItemArgs;
			ItemArgs.Add(TEXT("Name"), Entry.Key);
			ItemArgs.Add(TEXT("Count"), FText::AsNumber(Entry.Value, &FNumberFormattingOptions::DefaultNoGrouping()));
			List += FText::Format(TakenSummaryItemFormat, ItemArgs).ToString();
		}
		else
		{
			List += Entry.Key.ToString(); // одна штука — просто название, без «x1»
		}
	}

	if (Summary.Money > 0)
	{
		if (!List.IsEmpty())
		{
			List += Separator;
		}
		FFormatNamedArguments MoneyArgs;
		MoneyArgs.Add(TEXT("Count"), FText::AsNumber(Summary.Money, &FNumberFormattingOptions::DefaultNoGrouping()));
		MoneyArgs.Add(TEXT("Word"), PickMoneyWord(Summary.Money, MoneyWordOne, MoneyWordFew, MoneyWordMany));
		List += FText::Format(TakenSummaryMoneyFormat, MoneyArgs).ToString();
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("List"), FText::FromString(List));
	FText Line = FText::Format(TakenSummaryFormat, Args);

	// Хвост про огнестрел — в той же строке (ADR-063 п.2 живёт, второй всплывашки нет).
	if (Summary.bFirearmTaken && !TakenSummaryFirearmSuffix.IsEmpty())
	{
		Line = FText::FromString(Line.ToString() + TakenSummaryFirearmSuffix.ToString());
	}

	// п.3.4: если рюкзак принял не всё — говорим об этом ТОЙ ЖЕ строкой.
	if (Summary.bBackpackRefused)
	{
		FFormatNamedArguments FullArgs;
		FullArgs.Add(TEXT("Summary"), Line);
		Line = FText::Format(BackpackFullWithSummaryFormat, FullArgs);
	}
	return Line;
}

FCorpseLootTakenSummary UCorpseLootWidget::TakeAllFromGroup()
{
	FCorpseLootTakenSummary Summary;

	// Деньги всей группы — одним движением (в окне они одна плитка).
	Summary.Money = FMath::RoundToInt32(TakeMoneyToPlayer());

	for (UCorpseLootComponent* Member : GetGroupCorpses())
	{
		// GetLootItems отдаёт КОПИЮ списка — забор из тела по ходу перебора безопасен.
		for (AMasterInventoryItem* Item : Member->GetLootItems())
		{
			TakeItemToBackpack(Item, &Summary);
		}
	}
	return Summary;
}

void UCorpseLootWidget::ShowTakenSummary(const FCorpseLootTakenSummary& Summary)
{
	UOnboardingComponent* Onboarding = Player ? Player->GetOnboarding() : nullptr;
	if (!Onboarding)
	{
		return;
	}
	const FText Line = BuildTakenSummaryText(Summary);
	if (!Line.IsEmpty())
	{
		// Способ показа — тот же, что уже используется в игре для подбора предметов (п.3.3).
		Onboarding->ShowTransientHint(Line);
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
		// Забор по одной плитке — прежнее поведение (в том числе всплывашка про огнестрел).
		TakeItemToBackpack(TakenItem);
	}

	// Частичный обыск: остаток остаётся в телах, список пересобирается по факту.
	RefreshList();
}

void UCorpseLootWidget::HandleTakeAllClicked()
{
	// «Забрать всё» опустошает ВСЮ группу разом (издатель п.3.1, решение game-lead), а
	// игрок получает ОДНУ строку о том, что упало в рюкзак (п.3.3).
	const FCorpseLootTakenSummary Summary = TakeAllFromGroup();
	ShowTakenSummary(Summary);
	RefreshList();
}

void UCorpseLootWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast(); // мир закрывает контроллер (CloseCorpseLoot)
}
