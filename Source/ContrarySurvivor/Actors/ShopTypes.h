// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "AConsumableItem.h" // EConsumableType (тип расходника для товара)
#include "ShopTypes.generated.h"

class AMasterInventoryItem;

// Вид товара в магазине: предмет в рюкзак или пополнение патронов.
UENUM(BlueprintType)
enum class EShopEntryKind : uint8
{
	Item UMETA(DisplayName = "Item"),  // спавн предмета ItemClass -> в рюкзак
	Ammo UMETA(DisplayName = "Ammo")   // +AmmoAmount к резерву дальнобойного оружия игрока
};

/**
 * Позиция в прайс-листе торговца (GDD §7.6 — цены DRAFT на тюнинг).
 * Каталог торговца — массив таких записей; задаётся в конструкторе вендора (editor-
 * независимо) и тюнингуется в редакторе.
 *
 * Нейтральный тип (вынесен из бывшего ATraderNPC.h при переходе на IShopVendor, A2):
 * потребители — IShopVendor/AMasterTrader (каталог), AContrarySurvivorHUD (отрисовка),
 * APlayerCharacter (покупка). Ни от какого конкретного класса торговца не зависит.
 */
USTRUCT(BlueprintType)
struct FShopEntry
{
	GENERATED_BODY()

	// Отображаемое имя в магазине.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FString DisplayName;

	// Цена покупки (валюта). DRAFT по GDD §7.6.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	float Price = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	EShopEntryKind Kind = EShopEntryKind::Item;

	// Для Kind=Item: класс выдаваемого предмета (расходник/броня/оружие как inventory-item).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TSubclassOf<AMasterInventoryItem> ItemClass;

	// Если выдаётся расходник — выставить ему этот тип (еда/вода/аптечка).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	bool bApplyConsumableType = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	EConsumableType ConsumableType = EConsumableType::Food;

	// Для Kind=Ammo: сколько патронов добавить в резерв.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 AmmoAmount = 0;
};
