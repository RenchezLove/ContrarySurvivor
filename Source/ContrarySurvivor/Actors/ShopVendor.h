// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ShopTypes.h" // FShopEntry (тип возврата GetCatalog)
#include "ShopVendor.generated.h"

class AMasterInventoryItem;

/**
 * Интерфейс «торговца магазина» (A2: развязка магазина от конкретного класса торговца).
 *
 * До A2 контроллер/HUD были жёстко типизированы под ATraderNPC* (Cast<ATraderNPC>,
 * NearbyTrader/ShopTrader как ATraderNPC*). Теперь покупка/продажа/отрисовка работают
 * через этот интерфейс, а конкретным вендором выступает AMasterTrader (родитель BP_Trader).
 *
 * Нативный C++-интерфейс (по образцу IInteractableNPCInterface): методы — плейн-virtual
 * (НЕ UFUNCTION), т.к. вызовы только из C++ (PC/HUD) и один из них возвращает const-ссылку
 * на TArray, что несовместимо с BlueprintNativeEvent. Реализующий актёр держит каталог/цены.
 *
 * Контракт выведен по фактическим вызовам:
 *   GetCatalog()          — HUD::ArmBuySlider/ConfirmShopSlider/DrawShop, PC::OnQABuyCheapest;
 *   GetSellValue(Item)    — HUD::ArmSellSlider/DrawShop, PC::OnQASellFirstItem;
 *   GetAmmoSellPerRound() — HUD::ArmSellSlider (цена выкупа одного патрона для слайдера стака).
 */
UINTERFACE(MinimalAPI)
class UShopVendor : public UInterface
{
	GENERATED_BODY()
};

class IShopVendor
{
	GENERATED_BODY()

public:
	// Каталог товаров (для отрисовки магазина и покупки).
	virtual const TArray<FShopEntry>& GetCatalog() const = 0;

	// Цена выкупа предмета у игрока (DRAFT ~50% от цены покупки, по категории).
	virtual float GetSellValue(const AMasterInventoryItem* Item) const = 0;

	// Цена выкупа ОДНОГО патрона (для слайдера продажи стака патронов).
	virtual float GetAmmoSellPerRound() const = 0;
};
