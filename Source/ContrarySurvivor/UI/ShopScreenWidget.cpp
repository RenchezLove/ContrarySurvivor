// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/ShopScreenWidget.h"
#include "ContrarySurvivor/UI/ShopRowWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Actors/ShopTypes.h"
#include "ContrarySurvivor/Ads/AdService.h"       // Build 1.2: «Продать дороже» (ТЗ №2)
#include "ContrarySurvivor/Ads/AdGatingLogic.h"
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"
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
	if (SellAdButton)
	{
		SellAdButton->OnClicked.AddDynamic(this, &UShopScreenWidget::HandleSellAdClicked);
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
	// Build 1.2 (ТЗ №2 раздел 3): окно итога с двумя кнопками нужно ЛЮБОЙ продаже —
	// теперь и нестакающийся предмет открывает панель подтверждения (количество жёстко 1),
	// а не продаётся мгновенно кликом строки: в панели живут обычная кнопка продажи и
	// золотая «Продать дороже». Отказ (обычная продажа) ничем не блокируется.
	AAmmoItem* Ammo = Cast<AAmmoItem>(Item);

	bTransactionActive = true;
	bTransactionIsBuy = false;
	TransactionCatalogIndex = INDEX_NONE;
	TransactionItem = Item;
	TransactionUnitAmmo = 0;
	TransactionTitle = Item->GetItemDisplayText();
	if (Ammo)
	{
		TransactionUnitPrice = Trader->GetAmmoSellPerRound();
		TransactionQtyMax = FMath::Max(1, Ammo->StackCount);
		TransactionQty = TransactionQtyMax; // по умолчанию продать всё (STALKER-стиль, как Canvas)
	}
	else
	{
		TransactionUnitPrice = Trader->GetSellValue(Item);
		TransactionQtyMax = 1;
		TransactionQty = 1;
	}

	// Снимок лимита/кулдауна точки рекламы на момент открытия сделки (см. заголовок).
	bAdLimitOk = Player && Player->GetShopAdUsesToday() < SellAdDailyLimit;
	bAdCooldownOk = Player && Player->IsShopAdCooldownPassed(SellAdCooldownSeconds);

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
	bAdShownLogged = false;
	bAdNotShownLogged = false;

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
	// Порог теперь EditAnywhere на игроке (Build 1.2.1 В1: 360 с вместо константы 15 мин).
	if (!AdGating::IsPlaytimeGatePassed(Player->GetTotalPlayTimeSeconds(), Player->GetAdMinPlaytimeSeconds()))
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
