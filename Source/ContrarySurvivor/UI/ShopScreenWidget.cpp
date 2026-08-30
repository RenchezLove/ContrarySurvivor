// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/ShopScreenWidget.h"
#include "ContrarySurvivor/UI/ItemTileWidget.h"
#include "ContrarySurvivor/UI/InventoryScreenWidget.h" // ADR-082: окно-напарник (правая панель, Trade)
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h"  // ADR-082: поиск напарника через HUD владельца
#include "GameFramework/PlayerController.h"            // ADR-082: PC->GetHUD() в GetPartnerInventoryWidget
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/Ads/AdService.h"       // Build 1.2: «Продать дороже» (ТЗ №2)
#include "ContrarySurvivor/Ads/AdGatingLogic.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "AArmor.h"               // «+N% защиты» у позиций брони (как Canvas DrawShop, ADR-043)
#include "ContrarySurvivor/Data/ContraryItemLibrary.h" // ADR-077 п.12: витрина позиции из строки DT_Items
#include "AMasterInventoryItem.h"
#include "AAmmoItem.h"            // стак патронов -> транзакция количества; иконка позиции Ammo
#include "AConsumableItem.h"      // иконка расходника каталога по типу (Build 1.2.2, тайлы)
#include "UInventoryComponent.h"
#include "Blueprint/WidgetTree.h" // сетка плиток строится в дереве живого экрана
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"    // ADR-085: DimBorder гасится в паре окон (магазин сверху)
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/Texture2D.h"

void UShopScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Дефолт класса плитки — C++-плитка с кодовым деревом: экран работает без ассетов
	// (паттерн окна обыска); WBP_ItemTile назначает генератор/Ринат.
	if (!TileWidgetClass)
	{
		TileWidgetClass = UItemTileWidget::StaticClass();
	}

	if (CloseButton)
	{
		CloseButton->OnClicked.AddDynamic(this, &UShopScreenWidget::HandleCloseClicked);
	}
	else
	{
		// Без Close магазин закрывается только клавишей E / кнопкой ДЕЙСТВИЕ — играбельно,
		// но предупреждаем (BindWidgetOptional: не краш, ADR-048).
		UE_LOG(LogQA, Warning, TEXT("ShopScreenWidget: кубик CloseButton не найден в WBP_Shop"));
	}

	// ADR-085: в паре окон магазин рисуется ПОВЕРХ инвентаря (HUD даёт ему слой выше),
	// чтобы панель количества была настоящей модалкой по центру экрана. Полноэкранное
	// затемнение тогда обязано остаться ровно у НИЖНЕГО окна (правило ADR-084) — своё
	// гасим, иначе оно съест клики по правой панели (инвентарю). Запасной одиночный
	// режим (флаг выключен) — затемнение остаётся как нарисовано.
	if (DimBorder && bUseExternalInventoryPanel)
	{
		DimBorder->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (QtySlider)
	{
		QtySlider->OnValueChanged.AddDynamic(this, &UShopScreenWidget::HandleSliderValueChanged);
	}
	if (QtyMinusButton)
	{
		QtyMinusButton->OnClicked.AddDynamic(this, &UShopScreenWidget::HandleQtyMinusClicked);
	}
	if (QtyPlusButton)
	{
		QtyPlusButton->OnClicked.AddDynamic(this, &UShopScreenWidget::HandleQtyPlusClicked);
	}
	if (SliderConfirmButton)
	{
		SliderConfirmButton->OnClicked.AddDynamic(this, &UShopScreenWidget::HandleConfirmClicked);
	}
	if (SliderCancelButton)
	{
		SliderCancelButton->OnClicked.AddDynamic(this, &UShopScreenWidget::HandleCancelClicked);
	}
	if (SellAdButton)
	{
		SellAdButton->OnClicked.AddDynamic(this, &UShopScreenWidget::HandleSellAdClicked);
	}

	// Панель количества спрятана до первой транзакции.
	if (SliderPanel)
	{
		SliderPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Б8, п.5: снимок авторской раскладки половин окна — ДО того, как код что-то растянул
	// или спрятал. Возврат к двум половинам идёт только отсюда.
	if (const UCanvasPanelSlot* BuySlot = BuyList ? Cast<UCanvasPanelSlot>(BuyList->Slot) : nullptr)
	{
		BuyListLayout = BuySlot->GetLayout();
		bBuyListLayoutCaptured = true;
	}
	if (SellList)
	{
		SellListVisibility = SellList->GetVisibility();
	}
	if (SellHeaderText)
	{
		SellHeaderVisibility = SellHeaderText->GetVisibility();
	}
	else if (SellList && bExpandBuyListWhenBackpackEmpty)
	{
		// Заголовок половины рюкзака ищется по имени SellHeaderText. Нет такого кубика —
		// список спрячется, а подпись над ним останется висеть уже над чужими плитками
		// (та самая «ловушка скрытия», ADR-050). Предупреждаем, а не падаем.
		UE_LOG(LogQA, Warning,
			TEXT("ShopScreenWidget: кубик SellHeaderText не найден в WBP_Shop — при растяжке товаров ")
			TEXT("подпись над рюкзаком останется на экране. Переименуй подпись в SellHeaderText."));
	}

	// Б8, п.4: кнопки мельче предела под палец подрастают (крупнее — не трогаются).
	ApplyMinTouchSize();
}

void UShopScreenWidget::ApplyMinTouchSize()
{
	if (MinTouchSize <= 0.0f)
	{
		return;
	}

	auto RaiseButton = [this](UWidget* Button, const TCHAR* DebugName)
	{
		UCanvasPanelSlot* Slot = Button ? Cast<UCanvasPanelSlot>(Button->Slot) : nullptr;
		if (!Slot)
		{
			return; // кнопка лежит не на канвасе — размер задаёт её контейнер, не мы
		}

		// У слота-РАСТЯЖКИ (края привязаны к разным долям панели) поля Right/Bottom значат
		// отступы, а не размер: трогать их как размер нельзя — раскладка уедет.
		const FAnchorData Layout = Slot->GetLayout();
		if (!Layout.Anchors.Minimum.Equals(Layout.Anchors.Maximum))
		{
			return;
		}

		const FVector2D Size = Slot->GetSize();
		const FVector2D Raised(FMath::Max(Size.X, MinTouchSize), FMath::Max(Size.Y, MinTouchSize));
		if (!Raised.Equals(Size))
		{
			Slot->SetSize(Raised);
			UE_LOG(LogQA, Warning,
				TEXT("ShopScreenWidget: кнопка %s была %.0fx%.0f — увеличена до %.0fx%.0f (предел под палец %.0f)"),
				DebugName, Size.X, Size.Y, Raised.X, Raised.Y, MinTouchSize);
		}
		else
		{
			UE_LOG(LogQA, Log, TEXT("ShopScreenWidget: кнопка %s уже %.0fx%.0f — предел под палец соблюдён"),
				DebugName, Size.X, Size.Y);
		}
	};

	RaiseButton(CloseButton, TEXT("CloseButton"));
	RaiseButton(QtyMinusButton, TEXT("QtyMinusButton"));
	RaiseButton(QtyPlusButton, TEXT("QtyPlusButton"));
	RaiseButton(SliderConfirmButton, TEXT("SliderConfirmButton"));
	RaiseButton(SliderCancelButton, TEXT("SliderCancelButton"));
	RaiseButton(SellAdButton, TEXT("SellAdButton"));
}

void UShopScreenWidget::InitShop(TScriptInterface<IShopVendor> InTrader, APlayerCharacter* InPlayer)
{
	Trader = InTrader;
	Player = InPlayer;
	CloseTransaction(); // каждый визит к торговцу — с чистого состояния
	RefreshAll();
}

void UShopScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Деньги — каждый кадр (дёшево; меняются и извне транзакций, например QA-клавишей M).
	if (MoneyText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Amount"), FText::AsNumber(FMath::RoundToInt32(GetPlayerMoney())));
		MoneyText->SetText(FText::Format(MoneyFormat, Args));
	}

	// Б8, п.5. Число колонок считается по ЖИВОЙ ширине списка, а она известна только после
	// первого нарисованного кадра и меняется, когда товары растягиваются на всё окно.
	// Поэтому каждый кадр сверяем «сколько помещается сейчас» с «сколько построено» и при
	// расхождении пересобираем ОДИН раз: следующая сверка уже сходится. Ширина сама по себе
	// от числа плиток не зависит (её задаёт слот списка), так что качелей между двумя
	// значениями быть не может.
	if (Trader && Player)
	{
		// Нулевая ширина (список спрятан или ещё не рисовался) в сверке НЕ участвует —
		// иначе спрятанный список требовал бы пересборки каждый кадр.
		const float BuyWidth = GetListInnerWidth(BuyList);
		const float SellWidth = GetListInnerWidth(SellList);
		const bool bBuyChanged = BuyWidth > 0.0f && ComputeColumns(BuyWidth) != LastBuyColumns;
		const bool bSellChanged = SellWidth > 0.0f && ComputeColumns(SellWidth) != LastSellColumns;
		if (bBuyChanged || bSellChanged)
		{
			RefreshAll();
		}
	}
}

float UShopScreenWidget::GetListInnerWidth(const UScrollBox* List) const
{
	if (!List)
	{
		return 0.0f;
	}

	const float Width = static_cast<float>(List->GetCachedGeometry().GetLocalSize().X);
	if (Width <= 0.0f)
	{
		return 0.0f; // список ещё ни разу не рисовался — живой ширины нет
	}

	// Полоса прокрутки стоит В ОДНОМ РЯДУ с содержимым (SScrollBox), то есть отъедает
	// ширину, когда видна. Держим её запас всегда: иначе в списке, который прокручивается,
	// последняя колонка вылезала бы за край.
	const float ScrollBarWidth = static_cast<float>(List->GetScrollbarThickness().X);
	return FMath::Max(0.0f, Width - ScrollBarWidth);
}

int32 UShopScreenWidget::ComputeColumnsForWidth(float InnerWidth, float TileWidth, float Spacing,
	int32 FallbackColumns)
{
	if (InnerWidth <= 0.0f)
	{
		return FMath::Max(1, FallbackColumns); // ширина ещё не измерена — настройка-откат
	}

	// Ряд из N плиток занимает N*ширину плитки и (N-1) зазоров:
	// N*W + (N-1)*S <= InnerWidth  ->  N <= (InnerWidth + S) / (W + S).
	const float SafeTileWidth = FMath::Max(1.0f, TileWidth);
	const float SafeSpacing = FMath::Max(0.0f, Spacing);
	const int32 Fits = FMath::FloorToInt((InnerWidth + SafeSpacing) / (SafeTileWidth + SafeSpacing));
	return FMath::Max(1, Fits); // одна плитка помещается всегда, даже в узкое окно
}

int32 UShopScreenWidget::ComputeColumns(float InnerWidth) const
{
	return ComputeColumnsForWidth(InnerWidth, static_cast<float>(TileSize.X), TileSpacingX, TileColumns);
}

float UShopScreenWidget::GetPlayerMoney() const
{
	const UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
	return Stats ? Stats->GetMoney() : 0.0f;
}

UInventoryScreenWidget* UShopScreenWidget::GetPartnerInventoryWidget() const
{
	const APlayerController* PC = GetOwningPlayer();
	const AContrarySurvivorHUD* CSHUD = PC ? Cast<AContrarySurvivorHUD>(PC->GetHUD()) : nullptr;
	return CSHUD ? CSHUD->GetInventoryWidgetInstance() : nullptr;
}

void UShopScreenWidget::BeginSellFromInventory(AMasterInventoryItem* Item)
{
	if (bTransactionActive)
	{
		return; // панель количества модальна — тот же гейт, что в HandleTileAction
	}
	UInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!IsValid(Item) || !Inventory || !Inventory->GetInventoryItems().Contains(Item))
	{
		return; // предмета нет / не в рюкзаке (например, устаревшая плитка после двойного клика)
	}
	ArmSellTransaction(Item);
}

void UShopScreenWidget::RefreshAll()
{
	// Порядок важен: сперва рюкзак — по числу его плиток видно, пуста ли правая половина
	// окна; затем раскладка половин; и только потом товары.
	int32 SellTiles = 0;
	if (bUseExternalInventoryPanel)
	{
		// ADR-082, этап 3: рюкзак теперь ОТДЕЛЬНОЕ окно-напарник (тот же UInventoryScreenWidget,
		// что у кнопки СУМКА и у обыска) — собственный урезанный список не строим, кадры на
		// RebuildList(bBuyList=false) не тратим. UpdateListsLayout прячет SellList/SellHeaderText.
		if (SellList)
		{
			SellList->ClearChildren();
		}
	}
	else
	{
		const int32 SellColumns = ComputeColumns(GetListInnerWidth(SellList));
		SellTiles = RebuildList(/*bBuyList=*/false, SellColumns);
		LastSellColumns = SellColumns;
	}

	UpdateListsLayout(/*bBackpackEmpty=*/SellTiles == 0);

	// Ширину товаров берём ПОСЛЕ раскладки половин, но в кадре растяжки она ещё старая:
	// живая геометрия обновляется только к следующему кадру. Ничего страшного — тик увидит
	// расхождение и пересоберёт сетку с новым числом колонок (см. NativeTick).
	const int32 BuyColumns = ComputeColumns(GetListInnerWidth(BuyList));
	RebuildList(/*bBuyList=*/true, BuyColumns);

	LastBuyColumns = BuyColumns;
}

void UShopScreenWidget::UpdateListsLayout(bool bBackpackEmpty)
{
	bool bExpand = bExpandBuyListWhenBackpackEmpty && bBackpackEmpty;

	UCanvasPanelSlot* BuySlot = BuyList ? Cast<UCanvasPanelSlot>(BuyList->Slot) : nullptr;
	if (bExpand && !bBuyListLayoutCaptured)
	{
		bExpand = false; // список товаров лежит не на канвасе — растягивать нечего
	}
	if (bExpand && BuySlot)
	{
		// Растяжка правого края возможна, только если левый и правый края списка уже
		// привязаны к РАЗНЫМ долям панели (слот-растяжка). У слота с одной точкой привязки
		// поле Right значит ширину, и подмена доли сломала бы раскладку.
		if (BuyListLayout.Anchors.Minimum.X >= BuyListLayout.Anchors.Maximum.X)
		{
			UE_LOG(LogQA, Warning,
				TEXT("ShopScreenWidget: список товаров стоит одной точкой привязки — на пустой рюкзак не растягиваю"));
			bExpand = false;
		}
	}

	if (BuySlot)
	{
		FAnchorData Layout = BuyListLayout;
		if (bExpand)
		{
			// Правый край товаров — до правого края панели. Левый край, верх, низ и все
			// отступы остаются авторскими.
			Layout.Anchors.Maximum.X = 1.0f;
		}
		BuySlot->SetLayout(Layout);
	}

	// Половина рюкзака вместе со своим заголовком уходит с экрана целиком: либо старой
	// причиной (bExpand — растяжка на пустой рюкзак, bExpandBuyListWhenBackpackEmpty не
	// трогаем), либо новой (ADR-082: рюкзак теперь окно-напарник, свою половину не строим).
	const bool bHideSellHalf = bExpand || bUseExternalInventoryPanel;
	if (SellList)
	{
		SellList->SetVisibility(bHideSellHalf ? ESlateVisibility::Collapsed : SellListVisibility);
	}
	if (SellHeaderText)
	{
		SellHeaderText->SetVisibility(bHideSellHalf ? ESlateVisibility::Collapsed : SellHeaderVisibility);
	}
}

int32 UShopScreenWidget::RebuildList(bool bBuyList, int32 Columns)
{
	UScrollBox* List = bBuyList ? BuyList.Get() : SellList.Get();
	if (!List)
	{
		UE_LOG(LogQA, Warning, TEXT("ShopScreenWidget: кубик %s не найден в WBP_Shop — список не построен"),
			bBuyList ? TEXT("BuyList") : TEXT("SellList"));
		return 0;
	}

	List->ClearChildren();

	if (!TileWidgetClass)
	{
		UE_LOG(LogQA, Warning,
			TEXT("ShopScreenWidget: TileWidgetClass пуст — списки пустые (дефолт ставится в NativeOnInitialized)"));
		return 0;
	}
	if (!Trader || !Player || !WidgetTree)
	{
		return 0;
	}

	// Build 1.2.2 (Ринат: «иконки в сетке, как в сталкере или LDoE»): внутри прежнего
	// ScrollBox-кубика — сетка плиток; клик по плитке = прежняя кнопка Купить/Продать.
	UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>(
		UUniformGridPanel::StaticClass());
	// Зазоры между плитками: отступ кладётся на КАЖДУЮ ячейку со всех сторон, поэтому
	// берём половину — между соседними плитками складываются две половины и получается
	// заданное число, по краю сетки остаётся половина (та же схема, что в инвентаре).
	Grid->SetSlotPadding(FMargin(TileSpacingX * 0.5f, TileSpacingY * 0.5f));
	List->AddChild(Grid);

	APlayerController* PC = GetOwningPlayer();
	const float Money = GetPlayerMoney();
	const int32 GridColumns = FMath::Max(1, Columns);
	int32 TileIndex = 0;

	auto AddTileToGrid = [&](UItemTileWidget* Tile)
	{
		if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(
			Tile, TileIndex / GridColumns, TileIndex % GridColumns))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Top);
		}
		++TileIndex;
	};

	if (bBuyList)
	{
		// Каталог вендора — подписи те же, что у Canvas DrawShop (у брони «+N% защиты» из CDO).
		const TArray<FShopEntry>& Catalog = Trader->GetCatalog();
		for (int32 i = 0; i < Catalog.Num(); ++i)
		{
			const FShopEntry& E = Catalog[i];

			// ADR-077 п.12 (магазин «через раз» без иконок и с «+0%»): позиция, собранная из
			// таблицы предметов, несёт витрину В СТРОКЕ DT_Items — её класс общий
			// (BP_ArmorBase), и CDO класса про иконку и защиту конкретного тира не знает.
			// Строка главнее; CDO остаётся запасным путём легаси-позиций без ItemRow.
			const FContraryItemRow* Row = E.ItemRow.IsNone() ? nullptr : ContraryItems::FindRow(E.ItemRow);

			// Название позиции: переводимое, если задано; иначе откат на служебный ключ —
			// то же правило, что у самих предметов (ADR-050, порция 0).
			FText Name = E.DisplayText.IsEmpty() ? FText::FromString(E.DisplayName) : E.DisplayText;
			if (E.ItemClass && E.ItemClass->IsChildOf(AArmor::StaticClass()))
			{
				const AArmor* ArmorCDO = GetDefault<AArmor>(E.ItemClass);
				const float Protection = (Row && Row->ArmorProtection > 0.0f)
					? Row->ArmorProtection : ArmorCDO->GetArmorProtection();
				FFormatNamedArguments ArmorArgs;
				ArmorArgs.Add(TEXT("ItemName"), Name);
				ArmorArgs.Add(TEXT("Percent"),
					FText::AsNumber(FMath::RoundToInt32(Protection * 100.0f)));
				Name = FText::Format(ArmorBonusFormat, ArmorArgs);
			}

			// Иконка позиции каталога — предмета ещё НЕТ, берём вычислимую: строка таблицы —
			// первой; дальше как раньше: патроны — иконка пачки, расходник с типом — иконка
			// типа, прочее — иконка CDO класса (GetItemIcon).
			TSoftObjectPtr<UTexture2D> SoftIcon;
			if (Row && !Row->Icon.IsNull())
			{
				SoftIcon = Row->Icon;
			}
			else if (E.Kind == EShopEntryKind::Ammo)
			{
				SoftIcon = GetDefault<AAmmoItem>()->GetItemIcon();
			}
			else if (E.bApplyConsumableType)
			{
				SoftIcon = AConsumableItem::GetDefaultIcon(E.ConsumableType);
			}
			else if (E.ItemClass)
			{
				SoftIcon = GetDefault<AMasterInventoryItem>(E.ItemClass)->GetItemIcon();
			}

			FFormatNamedArguments PriceArgs;
			PriceArgs.Add(TEXT("Price"), FText::AsNumber(FMath::RoundToInt32(E.Price)));

			if (UItemTileWidget* Tile = CreateWidget<UItemTileWidget>(PC, TileWidgetClass))
			{
				Tile->CatalogIndex = i;
				Tile->SetTileData(SoftIcon.IsNull() ? nullptr : SoftIcon.LoadSynchronous(),
					Name, /*InCount=*/1);
				Tile->SetTileSize(TileSize, TileIconSize);
				Tile->SetPriceText(FText::Format(BuyPriceFormat, PriceArgs));
				const bool bAffordable = Money >= E.Price;
				Tile->SetActionEnabled(bAffordable);
				Tile->SetStatusText(bAffordable ? FText::GetEmpty() : NotEnoughMoneyText);
				Tile->OnTileClicked.AddUObject(this, &UShopScreenWidget::HandleTileAction);
				AddTileToGrid(Tile);
			}
		}
	}
	else
	{
		// Рюкзак игрока: валидные и не надетые (надетую броню из этого списка не продаём).
		UInventoryComponent* Inv = Player->GetInventory();
		if (!Inv)
		{
			return 0;
		}
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (!IsValid(Item) || Inv->IsItemEquipped(Item))
			{
				continue;
			}

			const float SellVal = Trader->GetSellValue(Item);

			FFormatNamedArguments PriceArgs;
			PriceArgs.Add(TEXT("Price"), FText::AsNumber(FMath::RoundToInt32(SellVal)));

			if (UItemTileWidget* Tile = CreateWidget<UItemTileWidget>(PC, TileWidgetClass))
			{
				Tile->Item = Item;
				const TSoftObjectPtr<UTexture2D> SoftIcon = Item->GetItemIcon();
				Tile->SetTileData(SoftIcon.IsNull() ? nullptr : SoftIcon.LoadSynchronous(),
					Item->GetItemDisplayText(), Item->GetStackCount());
				Tile->SetTileSize(TileSize, TileIconSize);
				Tile->SetPriceText(FText::Format(SellPriceFormat, PriceArgs));
				Tile->OnTileClicked.AddUObject(this, &UShopScreenWidget::HandleTileAction);
				AddTileToGrid(Tile);
			}
		}
	}

	return TileIndex; // сколько плиток реально построено (у рюкзака 0 = продавать нечего)
}

// ---------------------------------------------------------------------------
// Клики
// ---------------------------------------------------------------------------

void UShopScreenWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast(); // мир закрывает контроллер (CloseShop) — виджет только сообщает
}

void UShopScreenWidget::HandleTileAction(UItemTileWidget* Tile)
{
	if (!Tile || !Player || !Trader)
	{
		return;
	}
	if (bTransactionActive)
	{
		return; // панель количества модальна: пока открыта — списки не действуют (как Canvas)
	}

	if (Tile->CatalogIndex != INDEX_NONE)
	{
		ArmBuyTransaction(Tile->CatalogIndex);
	}
	else if (AMasterInventoryItem* Item = Tile->Item.Get())
	{
		ArmSellTransaction(Item);
	}
}

// ---------------------------------------------------------------------------
// Транзакция количества (модель Canvas-слайдера: Arm/Adjust/Confirm/Cancel)
// ---------------------------------------------------------------------------

void UShopScreenWidget::ArmBuyTransaction(int32 CatalogIndex)
{
	const TArray<FShopEntry>& Catalog = Trader->GetCatalog();
	if (!Catalog.IsValidIndex(CatalogIndex))
	{
		return;
	}
	const FShopEntry& E = Catalog[CatalogIndex];

	bTransactionActive = true;
	bTransactionIsBuy = true;
	TransactionCatalogIndex = CatalogIndex;
	TransactionItem = nullptr;
	TransactionUnitPrice = E.Price;
	TransactionUnitAmmo = (E.Kind == EShopEntryKind::Ammo) ? FMath::Max(0, E.AmmoAmount) : 0;
	TransactionTitle = E.DisplayText.IsEmpty() ? FText::FromString(E.DisplayName) : E.DisplayText;

	// Потолок по деньгам (минимум 1) — как Canvas ArmBuySlider.
	int32 ByMoney = 999;
	if (E.Price > 0.0f)
	{
		ByMoney = FMath::FloorToInt(GetPlayerMoney() / E.Price);
	}
	TransactionQtyMax = FMath::Clamp(ByMoney, 1, 999);
	TransactionQty = 1;

	if (SliderPanel)
	{
		SliderPanel->SetVisibility(ESlateVisibility::Visible);
	}
	// Списки на время транзакции гаснут (модальность панели количества). ADR-082: и
	// окно-напарник (правая панель, настоящий инвентарь) — иначе игрок кликнет по рюкзаку
	// поверх открытой панели количества.
	if (BuyList)  { BuyList->SetIsEnabled(false); }
	if (SellList) { SellList->SetIsEnabled(false); }
	if (UInventoryScreenWidget* Partner = GetPartnerInventoryWidget())
	{
		Partner->SetInteractionEnabled(false);
	}

	if (QtySlider)
	{
		bUpdatingSliderFromCode = true;
		QtySlider->SetMinValue(1.0f);
		QtySlider->SetMaxValue(static_cast<float>(TransactionQtyMax));
		QtySlider->SetStepSize(1.0f);
		QtySlider->SetValue(static_cast<float>(TransactionQty));
		bUpdatingSliderFromCode = false;
	}
	UpdateTransactionTexts();
}

int32 UShopScreenWidget::GetSellQtyMax(const AMasterInventoryItem* Item)
{
	if (!Item)
	{
		return 1;
	}
	// Стопкой торгуем по её размеру, нестакающейся вещью — ровно одной штукой. Счётчик
	// стопки живёт в базе AMasterInventoryItem, поэтому патроны здесь ничем не особенные:
	// у них тот же StackCount, что у воды, тушёнки, аптечек и шкур.
	return Item->IsStackable() ? FMath::Max(1, Item->GetStackCount()) : 1;
}

void UShopScreenWidget::ArmSellTransaction(AMasterInventoryItem* Item)
{
	// Build 1.2 (ТЗ №2 раздел 3): окно итога с двумя кнопками нужно ЛЮБОЙ продаже —
	// и нестакающийся предмет открывает панель подтверждения, а не продаётся мгновенно
	// кликом плитки: в панели живут обычная кнопка продажи и золотая «Продать дороже».
	// Отказ (обычная продажа) ничем не блокируется.
	AAmmoItem* Ammo = Cast<AAmmoItem>(Item);

	bTransactionActive = true;
	bTransactionIsBuy = false;
	TransactionCatalogIndex = INDEX_NONE;
	TransactionItem = Item;
	TransactionUnitAmmo = 0;
	TransactionTitle = Item->GetItemDisplayText();

	// Цена единицы: у патронов — спец-тариф за штуку, у прочих — цена выкупа предмета
	// (тариф категории и есть цена ОДНОЙ штуки). Ровно та же развилка, что в Canvas-пути.
	TransactionUnitPrice = Ammo ? Trader->GetAmmoSellPerRound() : Trader->GetSellValue(Item);

	// Предел количества — размер стопки для ЛЮБОГО стакающегося предмета (Build 1.2.2,
	// приёмка Рината 05-08). До этой правки стопку видели только патроны, из-за чего «Вода
	// х3» продавалась по одной штуке, а ползунок и кнопки плюс/минус стояли намертво.
	// Улучшение уже жило в Canvas-пути (AContrarySurvivorHUD::ArmSellSlider, ТЗ Г волны
	// 1.2.1) и просто не было перенесено сюда вместе с переходом на UMG.
	TransactionQtyMax = GetSellQtyMax(Item);
	TransactionQty = TransactionQtyMax; // по умолчанию продать всё (STALKER-стиль, как Canvas)

	// Снимок лимита/кулдауна точки рекламы на момент открытия сделки (см. заголовок). Правка
	// Рината 29.08: SellAdDailyLimit=0 означает «без ограничения» — иначе UsesToday() < 0
	// было бы ложью всегда (ограничение выключено, а условие никогда бы не пропускало).
	// SellAdCooldownSeconds=0 отдельной ветки не требует: AdGatingLogic::IsCooldownPassed
	// считает SecondsSince >= CooldownSeconds, при 0 это истинно уже в первое же мгновение.
	bAdLimitOk = Player && (SellAdDailyLimit <= 0 || Player->GetShopAdUsesToday() < SellAdDailyLimit);
	bAdCooldownOk = Player && Player->IsShopAdCooldownPassed(SellAdCooldownSeconds);

	if (SliderPanel)
	{
		SliderPanel->SetVisibility(ESlateVisibility::Visible);
	}
	// ADR-082: и окно-напарник (правая панель) гаснет вместе со списками — см. ArmBuyTransaction.
	if (BuyList)  { BuyList->SetIsEnabled(false); }
	if (SellList) { SellList->SetIsEnabled(false); }
	if (UInventoryScreenWidget* Partner = GetPartnerInventoryWidget())
	{
		Partner->SetInteractionEnabled(false);
	}

	if (QtySlider)
	{
		bUpdatingSliderFromCode = true;
		QtySlider->SetMinValue(1.0f);
		QtySlider->SetMaxValue(static_cast<float>(TransactionQtyMax));
		QtySlider->SetStepSize(1.0f);
		QtySlider->SetValue(static_cast<float>(TransactionQty));
		bUpdatingSliderFromCode = false;
	}
	UpdateTransactionTexts();
}

void UShopScreenWidget::CloseTransaction()
{
	bTransactionActive = false;
	bTransactionIsBuy = false;
	TransactionCatalogIndex = INDEX_NONE;
	TransactionItem = nullptr;
	TransactionQty = 1;
	TransactionQtyMax = 1;
	TransactionUnitPrice = 0.0f;
	TransactionUnitAmmo = 0;
	TransactionTitle = FText::GetEmpty();
	bAdShownLogged = false;
	bAdNotShownLogged = false;

	if (SliderPanel)
	{
		SliderPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (BuyList)  { BuyList->SetIsEnabled(true); }
	if (SellList) { SellList->SetIsEnabled(true); }
	// ADR-082: возвращаем интерактивность окну-напарнику вместе со списками.
	if (UInventoryScreenWidget* Partner = GetPartnerInventoryWidget())
	{
		Partner->SetInteractionEnabled(true);
	}
}

void UShopScreenWidget::SetTransactionQty(int32 NewQty)
{
	if (!bTransactionActive)
	{
		return;
	}
	TransactionQty = FMath::Clamp(NewQty, 1, FMath::Max(1, TransactionQtyMax));

	if (QtySlider)
	{
		bUpdatingSliderFromCode = true;
		QtySlider->SetValue(static_cast<float>(TransactionQty));
		bUpdatingSliderFromCode = false;
	}
	UpdateTransactionTexts();
}

void UShopScreenWidget::UpdateTransactionTexts()
{
	if (SliderTitleText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ItemName"), TransactionTitle);
		SliderTitleText->SetText(FText::Format(
			bTransactionIsBuy ? BuyTitleFormat : SellTitleFormat, Args));
	}
	if (SliderQtyText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Qty"), FText::AsNumber(TransactionQty));
		Args.Add(TEXT("Max"), FText::AsNumber(TransactionQtyMax));
		SliderQtyText->SetText(FText::Format(QtyFormat, Args));
	}

	// Пересчёт пачек в патроны есть ТОЛЬКО при покупке патронов. Прячем строку целиком
	// вместе с подписью Рината (контейнер), иначе подпись висела бы при покупке аптечки.
	const bool bShowAmmoLine = (TransactionUnitAmmo > 0);
	const ESlateVisibility AmmoLineVisibility =
		bShowAmmoLine ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	if (SliderQtyAmmoRow)
	{
		SliderQtyAmmoRow->SetVisibility(AmmoLineVisibility);
	}
	else if (SliderQtyAmmoText)
	{
		// Фолбэк для раскладки без контейнера: прячем только само значение.
		SliderQtyAmmoText->SetVisibility(AmmoLineVisibility);
	}
	if (SliderQtyAmmoText && bShowAmmoLine)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Rounds"), FText::AsNumber(TransactionQty * TransactionUnitAmmo));
		SliderQtyAmmoText->SetText(FText::Format(QtyAmmoFormat, Args));
	}

	if (SliderTotalText)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Total"), FText::AsNumber(
			FMath::RoundToInt32(TransactionUnitPrice * static_cast<float>(TransactionQty))));
		SliderTotalText->SetText(FText::Format(
			bTransactionIsBuy ? TotalFormat : RevenueFormat, Args));
	}

	// Build 1.2: золотая кнопка «Продать дороже» — числа живые от текущей суммы сделки
	// (ТЗ №2 раздел 3), условия показа пересчитываются с каждым движением ползунка.
	UpdateSellAdButton();
}

void UShopScreenWidget::UpdateSellAdButton()
{
	if (!SellAdButton)
	{
		return; // кубика в WBP_Shop нет — точка рекламы недоступна, магазин работает как раньше
	}

	// Кнопка живёт ТОЛЬКО в продаже (ТЗ №2 п.7: при покупке рекламу не предлагать).
	if (!bTransactionActive || bTransactionIsBuy || !Player)
	{
		SellAdButton->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const int32 Qty = FMath::Clamp(TransactionQty, 1, FMath::Max(1, TransactionQtyMax));
	const float BaseTotal = TransactionUnitPrice * static_cast<float>(Qty);
	const float Mult = FMath::Max(1.0f, SellAdBonusMultiplier);
	IAdService* Ads = AdService::Get(this);

	// Условия показа (ТЗ №2 п.4): все обязаны выполниться, иначе кнопка прячется целиком.
	FString DenyReason;
	// ADR-063 п.3 (РИ-29): 300 с игры ИЛИ сдан первый квест — что раньше. Имя причины
	// «under_15min» историческое, это идентификатор события аналитики — не переименовывать.
	if (!AdGating::IsAdGatePassed(Player->GetTotalPlayTimeSeconds(),
		Player->HasTurnedInFirstQuest(), Player->GetAdMinPlaytimeSeconds()))
	{
		DenyReason = TEXT("under_15min");
	}
	else if (BaseTotal < SellAdMinTotal)
	{
		DenyReason = TEXT("below_min_total");
	}
	else if (!Ads || !Ads->IsRewardedReady())
	{
		DenyReason = TEXT("no_ad");
	}
	else if (!bAdLimitOk)
	{
		DenyReason = TEXT("limit");
	}
	else if (!bAdCooldownOk)
	{
		DenyReason = TEXT("cooldown");
	}

	const bool bShow = DenyReason.IsEmpty();
	SellAdButton->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (bShow)
	{
		if (SellAdText)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Bonus"), FText::AsNumber(FMath::RoundToInt32(BaseTotal * Mult)));
			Args.Add(TEXT("Base"), FText::AsNumber(FMath::RoundToInt32(BaseTotal)));
			SellAdText->SetText(FText::Format(SellAdPriceFormat, Args));
		}
		if (SellAdSubText)
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Percent"), FText::AsNumber(FMath::RoundToInt32((Mult - 1.0f) * 100.0f)));
			SellAdSubText->SetText(FText::Format(SellAdSubFormat, Args));
		}
	}

	// Аналитика — один раз на транзакцию (ползунок дёргает пересчёт на каждое движение).
	UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this);
	if (Analytics && bShow && !bAdShownLogged)
	{
		bAdShownLogged = true;
		Analytics->RecordAdStage(TEXT("shop"), TEXT("button_shown"), BaseTotal, /*bWithValue=*/true);
	}
	else if (Analytics && !bShow && !bAdNotShownLogged && !bAdShownLogged)
	{
		bAdNotShownLogged = true;
		Analytics->RecordAdNotShown(TEXT("shop"), DenyReason);
	}
}

void UShopScreenWidget::HandleSliderValueChanged(float NewValue)
{
	if (bUpdatingSliderFromCode)
	{
		return; // это наш же SetValue — не зацикливаемся
	}
	SetTransactionQty(FMath::RoundToInt(NewValue));
}

void UShopScreenWidget::HandleQtyMinusClicked()
{
	SetTransactionQty(TransactionQty - 1);
}

void UShopScreenWidget::HandleQtyPlusClicked()
{
	SetTransactionQty(TransactionQty + 1);
}

void UShopScreenWidget::HandleConfirmClicked()
{
	if (bAdInProgress)
	{
		return; // идёт «ролик» — панель заморожена до его исхода
	}
	if (!bTransactionActive || !Player || !Trader)
	{
		CloseTransaction();
		return;
	}

	const int32 Qty = FMath::Clamp(TransactionQty, 1, FMath::Max(1, TransactionQtyMax));

	// Сама покупка/продажа — СУЩЕСТВУЮЩИЕ методы игрока (расчёты не дублируем, ADR-048).
	if (bTransactionIsBuy)
	{
		const TArray<FShopEntry>& Catalog = Trader->GetCatalog();
		if (Catalog.IsValidIndex(TransactionCatalogIndex))
		{
			Player->Shop_BuyEntryQty(Catalog[TransactionCatalogIndex], Qty);
		}
	}
	else if (IsValid(TransactionItem))
	{
		Player->Shop_SellItemQty(TransactionItem, TransactionUnitPrice, Qty);
	}

	CloseTransaction();
	RefreshAll();
	// ADR-082: деньги и состав рюкзака сменились — окно-напарник тоже просит пересчёт.
	if (UInventoryScreenWidget* Partner = GetPartnerInventoryWidget())
	{
		Partner->RefreshInventoryDisplay();
	}
}

void UShopScreenWidget::HandleCancelClicked()
{
	if (bAdInProgress)
	{
		return;
	}
	CloseTransaction();
}

void UShopScreenWidget::HandleSellAdClicked()
{
	if (bAdInProgress || !bTransactionActive || bTransactionIsBuy || !Player || !IsValid(TransactionItem))
	{
		return;
	}

	IAdService* Ads = AdService::Get(this);
	if (!Ads || !Ads->IsRewardedReady())
	{
		// Ролик разгрузился между показом кнопки и кликом — честно прячем кнопку.
		if (SellAdButton)
		{
			SellAdButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	UE_LOG(LogQA, Display, TEXT("QA: shop sell-ad button clicked (total %.0f x%.2f)"),
		TransactionUnitPrice * TransactionQty, SellAdBonusMultiplier);
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordAdStage(TEXT("shop"), TEXT("button_clicked"));
		Analytics->RecordAdStage(TEXT("shop"), TEXT("started"));
	}

	bAdInProgress = true;
	Ads->ShowRewarded(AdPlacements::ShopSellBonus,
		FSimpleDelegate::CreateUObject(this, &UShopScreenWidget::HandleShopAdSuccess),
		FSimpleDelegate::CreateUObject(this, &UShopScreenWidget::HandleShopAdFail));
}

void UShopScreenWidget::HandleShopAdSuccess()
{
	bAdInProgress = false;
	if (!bTransactionActive || !Player || !IsValid(TransactionItem))
	{
		CloseTransaction();
		return;
	}

	// Досмотрел: та же партия уходит по цене с надбавкой (ТЗ №2 п.5). Продажа — тем же
	// существующим методом игрока, только цена единицы умножена.
	const int32 Qty = FMath::Clamp(TransactionQty, 1, FMath::Max(1, TransactionQtyMax));
	const float Mult = FMath::Max(1.0f, SellAdBonusMultiplier);
	const float BaseTotal = TransactionUnitPrice * static_cast<float>(Qty);
	const float BoostedTotal = BaseTotal * Mult;
	Player->Shop_SellItemQty(TransactionItem, TransactionUnitPrice * Mult, Qty);
	Player->RegisterShopAdUse();

	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		// value = начисленная сумма; базовая — в лог QA (GA несёт одно число).
		Analytics->RecordAdStage(TEXT("shop"), TEXT("completed"), BoostedTotal, /*bWithValue=*/true);
	}
	UE_LOG(LogQA, Display, TEXT("QA: shop sell-ad completed - base %.0f, credited %.0f"),
		BaseTotal, BoostedTotal);

	CloseTransaction();
	RefreshAll();
	// ADR-082: деньги и состав рюкзака сменились — окно-напарник тоже просит пересчёт.
	if (UInventoryScreenWidget* Partner = GetPartnerInventoryWidget())
	{
		Partner->RefreshInventoryDisplay();
	}
}

void UShopScreenWidget::HandleShopAdFail()
{
	bAdInProgress = false;

	// Закрыл досрочно: сделка НЕ отменяется и НЕ проводится — игрок возвращается в окно
	// продажи в исходном состоянии и сам решает, продавать ли по обычной цене (ТЗ №2 п.5).
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordAdStage(TEXT("shop"), TEXT("dismissed"));
	}
	if (SellAdSubText)
	{
		SellAdSubText->SetText(AdNotFinishedText);
	}
}
