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

	// СЛУЖЕБНЫЙ КЛЮЧ позиции: уходит в ItemName купленного предмета, по нему сходится
	// зачёт квеста. НЕ переводится, значения не менять (ADR-050, порция 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FString DisplayName;

	// ПЕРЕВОДИМОЕ название позиции, которое видит игрок в списке товаров и в рюкзаке после
	// покупки. Пусто -> откат на ключ выше (сегодняшнее поведение, ничего не пропадает).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FText DisplayText;

	// Внутренний ЛАТИНСКИЙ идентификатор позиции для событий аналитики (shop:buy:<id>).
	// Разведён с DisplayName (лут бандитов, 07-17): имена товаров теперь русские, а
	// SanitizeEventPart аналитики пропускает только латиницу/цифры — кириллица выродилась бы
	// в подчёркивания и разные товары слиплись бы в одно событие. Пусто -> берётся DisplayName.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FString AnalyticsId;

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

	// ADR-075: имя строки таблицы предметов DT_Items, из которой собрана эта позиция.
	// NAME_None = позиция собрана по-старому (без таблицы). Непустое — покупка накладывает
	// на купленный предмет данные строки (ключ/название/иконку/меш/стак/броню) через
	// ContraryItems::ApplyRowToItem, а не только имя и тип, как раньше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FName ItemRow;
};

/**
 * Ссылка позиции каталога торговца на строку таблицы предметов DT_Items (ADR-075, спека
 * spec-datatables-phase2.md, группа 5). Каталог торговца из массива таких ссылок строит
 * AMasterTrader::RebuildCatalog: цена и вид позиции берутся из строки таблицы, поэтому
 * правки каталога в редакторе больше НЕ перетираются кодом (прежняя боль: RebuildCatalog
 * пересобирал массив Catalog в BeginPlay и стирал правки).
 */
USTRUCT(BlueprintType)
struct FShopCatalogRef
{
	GENERATED_BODY()

	// Имя строки таблицы предметов (water_bottle, knife, armor_t1_head, …).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (DisplayPriority = "1",
		DisplayName = "Строка таблицы предметов"))
	FName ItemRow;

	// Цена ИМЕННО у этого торговца. Меньше нуля = брать цену из строки таблицы (обычный
	// случай); ноль и больше — перекрыть (задел под «дорогого» торговца).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (DisplayPriority = "2",
		DisplayName = "Цена у этого торговца (<0 = из таблицы)"))
	float PriceOverride = -1.0f;
};
