// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/ShopScreenWidget.h"
#include "ContrarySurvivor/UI/ShopRowWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "AArmor.h"               // «+N% защиты» у позиций брони (как Canvas DrawShop, ADR-043)
#include "AMasterInventoryItem.h"
#include "AAmmoItem.h"            // стак патронов -> транзакция количества
#include "UInventoryComponent.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"

void UShopScreenWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

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

	// Панель количества спрятана до первой транзакции.
	if (SliderPanel)
	{
		SliderPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
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
}

float UShopScreenWidget::GetPlayerMoney() const
{
	const UStatsComponent* Stats = Player ? Player->GetStats() : nullptr;
	return Stats ? Stats->GetMoney() : 0.0f;
}

void UShopScreenWidget::RefreshAll()
{
	RebuildList(/*bBuyList=*/true);
	RebuildList(/*bBuyList=*/false);
}

void UShopScreenWidget::RebuildList(bool bBuyList)
{
	UScrollBox* List = bBuyList ? BuyList.Get() : SellList.Get();
	if (!List)
	{
		UE_LOG(LogQA, Warning, TEXT("ShopScreenWidget: кубик %s не найден в WBP_Shop — список не построен"),
			bBuyList ? TEXT("BuyList") : TEXT("SellList"));
		return;
	}

	List->ClearChildren();

	if (!RowWidgetClass)
	{
		UE_LOG(LogQA, Warning,
			TEXT("ShopScreenWidget: RowWidgetClass пуст (назначь WBP_ShopRow в Class Defaults WBP_Shop) — списки пустые"));
		return;
	}
	if (!Trader || !Player)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	const float Money = GetPlayerMoney();

	if (bBuyList)
	{
		// Каталог вендора — метки строк те же, что у Canvas DrawShop (у брони «+N% защиты» из CDO).
		const TArray<FShopEntry>& Catalog = Trader->GetCatalog();
		for (int32 i = 0; i < Catalog.Num(); ++i)
		{
			const FShopEntry& E = Catalog[i];

			// Название позиции: переводимое, если задано; иначе откат на служебный ключ —
			// то же правило, что у самих предметов (ADR-050, порция 0).
			FText Name = E.DisplayText.IsEmpty() ? FText::FromString(E.DisplayName) : E.DisplayText;
			if (E.ItemClass && E.ItemClass->IsChildOf(AArmor::StaticClass()))
			{
				const AArmor* ArmorCDO = GetDefault<AArmor>(E.ItemClass);
				FFormatNamedArguments ArmorArgs;
				ArmorArgs.Add(TEXT("ItemName"), Name);
				ArmorArgs.Add(TEXT("Percent"),
					FText::AsNumber(FMath::RoundToInt32(ArmorCDO->GetArmorProtection() * 100.0f)));
				Name = FText::Format(ArmorBonusFormat, ArmorArgs);
			}

			FFormatNamedArguments PriceArgs;
			PriceArgs.Add(TEXT("Price"), FText::AsNumber(FMath::RoundToInt32(E.Price)));

			if (UShopRowWidget* Row = CreateWidget<UShopRowWidget>(PC, RowWidgetClass))
			{
				Row->CatalogIndex = i;
				Row->SetupRow(Name, FText::Format(BuyPriceFormat, PriceArgs), BuyActionText,
					/*bActionEnabled=*/Money >= E.Price);
				Row->OnActionClicked.AddUObject(this, &UShopScreenWidget::HandleRowAction);
				List->AddChild(Row);
			}
		}
	}
	else
	{
		// Рюкзак игрока: валидные и не надетые (надетую броню из этого списка не продаём).
		UInventoryComponent* Inv = Player->GetInventory();
		if (!Inv)
		{
			return;
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

			if (UShopRowWidget* Row = CreateWidget<UShopRowWidget>(PC, RowWidgetClass))
			{
				Row->SellItem = Item;
				Row->SetupRow(Item->GetItemDisplayText(),
					FText::Format(SellPriceFormat, PriceArgs), SellActionText,
					/*bActionEnabled=*/true);
				Row->OnActionClicked.AddUObject(this, &UShopScreenWidget::HandleRowAction);
				List->AddChild(Row);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Клики
// ---------------------------------------------------------------------------

void UShopScreenWidget::HandleCloseClicked()
{
	OnCloseRequested.Broadcast(); // мир закрывает контроллер (CloseShop) — виджет только сообщает
}

void UShopScreenWidget::HandleRowAction(UShopRowWidget* Row)
{
	if (!Row || !Player || !Trader)
	{
		return;
	}
	if (bTransactionActive)
	{
		return; // панель количества модальна: пока открыта — списки не действуют (как Canvas)
	}

	if (Row->CatalogIndex != INDEX_NONE)
	{
		ArmBuyTransaction(Row->CatalogIndex);
	}
	else if (AMasterInventoryItem* Item = Row->SellItem.Get())
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
	// Списки на время транзакции гаснут (модальность панели количества).
	if (BuyList)  { BuyList->SetIsEnabled(false); }
	if (SellList) { SellList->SetIsEnabled(false); }

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

void UShopScreenWidget::ArmSellTransaction(AMasterInventoryItem* Item)
{
	// Панель количества имеет смысл только для стака патронов; прочее продаётся сразу
	// (та же логика, что Canvas ArmSellSlider).
	AAmmoItem* Ammo = Cast<AAmmoItem>(Item);
	if (!Ammo)
	{
		Player->Shop_SellItem(Item, Trader->GetSellValue(Item));
		RefreshAll();
		return;
	}

	bTransactionActive = true;
	bTransactionIsBuy = false;
	TransactionCatalogIndex = INDEX_NONE;
	TransactionItem = Item;
	TransactionUnitPrice = Trader->GetAmmoSellPerRound();
	TransactionUnitAmmo = 0;
	TransactionTitle = Item->GetItemDisplayText();
	TransactionQtyMax = FMath::Max(1, Ammo->StackCount);
	TransactionQty = TransactionQtyMax; // по умолчанию продать всё (STALKER-стиль, как Canvas)

	if (SliderPanel)
	{
		SliderPanel->SetVisibility(ESlateVisibility::Visible);
	}
	if (BuyList)  { BuyList->SetIsEnabled(false); }
	if (SellList) { SellList->SetIsEnabled(false); }

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

	if (SliderPanel)
	{
		SliderPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (BuyList)  { BuyList->SetIsEnabled(true); }
	if (SellList) { SellList->SetIsEnabled(true); }
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
}

void UShopScreenWidget::HandleCancelClicked()
{
	CloseTransaction();
}
