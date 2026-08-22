// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"   // TSoftObjectPtr / TSoftClassPtr (мягкие ссылки строк)
#include "Engine/DataTable.h"        // FTableRowBase
#include "AMasterInventoryItem.h"    // EItemCategory + класс предмета (TSoftClassPtr)
#include "AConsumableItem.h"         // EConsumableType
#include "AArmor.h"                  // EArmorSlot
#include "ContraryItemRow.generated.h"

class UTexture2D;
class UStaticMesh;
class USkeletalMesh;

/**
 * Строка таблицы-реестра предметов DT_Items (ADR-075 п.2, спека spec-datatables-phase2.md).
 *
 * ПРИНЦИП ВЛАДЕНИЯ (анти-дубль, п.5 ТЗ издателя): здесь живут идентичность, витрина и
 * экономика предмета (ключ, название, иконка, меш, цена, категория, стак, слот/защита брони).
 * ПОВЕДЕНИЕ (боевые числа оружия, величины лечения расходников, тайминги) остаётся на
 * классах/BP и в таблицу НЕ дублируется.
 *
 * ИМЯ СТРОКИ (RowName) — латинский идентификатор в нижнем регистре, СОВПАДАЕТ с AnalyticsId
 * каталога торговца (water_bottle, canned_food, bandage, ammo_9mm, knife, pistol,
 * armor_t1_head … armor_t3_pants; новые: wolf_pelt, laptop). Решение лида 22.08:
 * кириллицу в имена строк не заводим.
 *
 * СВЯЗЬ СО СТАРЫМИ КЛЮЧАМИ (ADR-050/069): колонка LegacyKey хранит прежний служебный ключ
 * (AMasterInventoryItem::ItemName) ДОСЛОВНО — рантайм-сравнения квестов/стаков продолжают
 * ходить по нему, и связка «ид строки ↔ старый ключ» существует ровно в одном месте.
 * Новой связки по ключам не появляется.
 *
 * Заполняет таблицу unreal-operator в редакторе; значения переезжают из конструкторов
 * (AArmorTiers.cpp, APistol.cpp, AMeleeWeapon.cpp, AAmmoItem.cpp, статики AConsumableItem)
 * и из RebuildCatalog торговца — см. таблицу соответствия в спеке.
 */
USTRUCT(BlueprintType)
struct FContraryItemRow : public FTableRowBase
{
	GENERATED_BODY()

	// Служебный ключ ADR-050 как есть («Pistol», «Патроны 9мм», «Шкура волка»…). По нему
	// сходятся квесты (FQuest::RequiredItemName) и слияние стаков. НЕ переводится, значения
	// не менять. Пусто = ключом служит имя строки (путь для НОВЫХ предметов).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (DisplayPriority = "1",
		DisplayName = "Служебный ключ (старый, для квестов)",
		ToolTip = "Прежний внутренний ключ предмета — по нему засчитываются квесты и сливаются стаки. НЕ переводится и не меняется. Пусто = ключом служит имя строки."))
	FString LegacyKey;

	// Переводимое название, которое видит игрок («Пистолет», «Шкура волка»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (DisplayPriority = "2",
		DisplayName = "Название для игрока"))
	FText DisplayText;

	// Иконка для инвентаря/магазина. Мягкая ссылка: текстуры может не быть — UI живёт на
	// текстовом фолбэке (как ItemIcon предмета, ADR-043).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (DisplayPriority = "3",
		DisplayName = "Иконка"))
	TSoftObjectPtr<UTexture2D> Icon;

	// Меш предмета в мире/в руке (SM_Pistol, SM_Knife). Пусто = меш класса/BP как раньше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (DisplayPriority = "4",
		DisplayName = "Меш в мире (StaticMesh)"))
	TSoftObjectPtr<UStaticMesh> WorldMesh;

	// Слотовый скелетный меш брони (SK_Armor_*). Только для брони; пусто = меш класса.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (DisplayPriority = "5",
		DisplayName = "Меш экипировки брони (SkeletalMesh)"))
	TSoftObjectPtr<USkeletalMesh> EquipMesh;

	// Класс актора предмета (C++ или BP). Спавн из строки идёт этим классом; строка сверху
	// накладывает ключ/название/иконку/меш/стак (ContraryItemLibrary::SpawnItemFromRow).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (DisplayPriority = "6",
		DisplayName = "Класс актора предмета"))
	TSoftClassPtr<AMasterInventoryItem> ItemClass;

	// Категория (влияет на потерю рюкзака при смерти, GDD §7.8). Обязана соответствовать
	// классу (квест-предмет — Quest, броня — Armor): заполняющий таблицу отвечает за сходство.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (DisplayPriority = "7",
		DisplayName = "Категория"))
	EItemCategory Category = EItemCategory::Resource;

	// Максимум штук в стаке. 0 = не трогать (остаётся значение класса); 1 = не стакается.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item", meta = (ClampMin = "0", DisplayPriority = "8",
		DisplayName = "Лимит стака (0 = как у класса)"))
	int32 MaxStackCount = 0;

	// Цена ПОКУПКИ у торговца. 0 = у торговца не продаётся (лут/квестовые вещи).
	// Цена выкупа считается от неё долей BuybackPriceFraction торговца — как сейчас.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (ClampMin = "0.0", DisplayPriority = "9",
		DisplayName = "Цена покупки (0 = не продаётся)"))
	float Price = 0.0f;

	// Тип расходника (действует только если класс — AConsumableItem/наследник):
	// еда/вода/аптечка. Величины восстановления — на классе и UStatsComponent (поведение).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Consumable", meta = (DisplayPriority = "10",
		DisplayName = "Тип расходника (для расходников)"))
	EConsumableType ConsumableType = EConsumableType::Food;

	// Слот брони (действует только если класс — AArmor/наследник).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor", meta = (DisplayPriority = "11",
		DisplayName = "Слот брони (для брони)"))
	EArmorSlot ArmorSlot = EArmorSlot::Head;

	// Доля снижения урона слотом [0..1] (для брони; тиры: Т1=0.05 / Т2=0.10 / Т3=0.16).
	// 0 = не трогать значение класса. Для брони защита — суть ПРЕДМЕТА (без неё «новый тир
	// без кода» невозможен), поэтому живёт здесь, а не на классе (решение спеки, §а).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "12",
		DisplayName = "Защита слота 0..1 (для брони; 0 = как у класса)"))
	float ArmorProtection = 0.0f;

	// Действующий служебный ключ строки: LegacyKey, а если он пуст — имя строки.
	// InRowName передаёт вызывающий (сама строка своего имени не знает — оно живёт в таблице).
	FString GetEffectiveKey(FName InRowName) const
	{
		return LegacyKey.IsEmpty() ? InRowName.ToString() : LegacyKey;
	}
};
