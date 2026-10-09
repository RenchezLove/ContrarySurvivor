// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AMasterInventoryItem.h"
#include "AAmmoItem.generated.h"

/**
 * Патроны как СТАК-предмет рюкзака (Фаза 5, экономика STALKER 2-стиль).
 *
 * Раньше патроны жили только числом на оружии (CurrentAmmoReserve). Теперь игрок носит
 * пачку патронов в рюкзаке (один актор-предмет со счётчиком StackCount), а резерв оружия
 * пополняется из этой пачки при перезарядке/покупке. Это позволяет складировать запас,
 * показывать слайдер купли-продажи и не плодить по актору на каждый патрон.
 *
 * Категория = Resource (как «запас»): частично теряется при смерти, как прочие ресурсы
 * (GDD §7.8). Use() — no-op: патроны не «используются» из инвентаря, а заряжаются в оружие
 * штатной перезарядкой (R) через APlayerCharacter::ReloadCurrentWeapon.
 */
/**
 * Виды патронов (двустволка, 09.10.2026). Вид патрона — это ИМЯ СТРОКИ таблицы предметов
 * DT_Items (ammo_9mm, ammo_12g): отдельного перечисления нет, новый калибр заводится строкой
 * таблицы без правки кода. Пустое имя везде означает прежний пистолетный патрон — так
 * оружие и сохранения, записанные до появления видов, продолжают работать без переделки.
 */
namespace ContraryAmmo
{
	// Прежний единственный патрон (пистолетный): вид по умолчанию для оружия и пачек.
	inline FName DefaultType()
	{
		static const FName Name(TEXT("ammo_9mm"));
		return Name;
	}

	// Дробовой патрон 12 калибра (двустволка).
	inline FName Shotgun12gType()
	{
		static const FName Name(TEXT("ammo_12g"));
		return Name;
	}

	// Пустой вид = вид по умолчанию.
	inline FName Normalize(FName AmmoType)
	{
		return AmmoType.IsNone() ? DefaultType() : AmmoType;
	}
}

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AAmmoItem : public AMasterInventoryItem
{
	GENERATED_BODY()

public:
	AAmmoItem();

	// Вид патрона ЭТОГО КЛАССА пачки (имя строки таблицы предметов). Работает, когда пачка
	// создана голым классом без строки таблицы; если строка у пачки есть — вид задаёт она
	// (см. GetAmmoType). По умолчанию — пистолетный патрон.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Ammo", meta = (DisplayPriority = "1",
		DisplayName = "Вид патрона (строка таблицы предметов)",
		ToolTip = "К какому оружию подходят патроны этой пачки: имя строки таблицы предметов (ammo_9mm, ammo_12g). Должно совпадать с полем «Вид патрона» у оружия. Если у пачки задана строка таблицы, вид берётся из неё."))
	FName AmmoType;

	// Вид патрона этой пачки: строка таблицы, из которой она собрана, а без строки — вид
	// класса. По нему перезарядка, счётчик на экране и покупка отличают дробь от пистолетных.
	UFUNCTION(BlueprintPure, Category = "Item|Ammo")
	FName GetAmmoType() const;

	// СЧЁТЧИК СТАКА (StackCount/MaxStackCount) ПОДНЯТ В БАЗУ AMasterInventoryItem
	// (Build 1.2.1, ТЗ Г: шкура/тушёнка/аптечка стакаются «по механизму патронов»).
	// Дефолты пачки патронов (пустая пачка, лимит 999) выставляет конструктор.

	// Патроны не используются из инвентаря напрямую — заряжаются перезарядкой. Use() — no-op.
	virtual void Use() override;
};

/**
 * Пачка дробовых патронов 12 калибра (двустволка, 09.10.2026). Отличается от базовой пачки
 * только видом патрона, служебным ключом и названием: всё поведение (стак, перезарядка,
 * торговля, сохранение) — общее. Служебный ключ совпадает с именем строки таблицы
 * (ammo_12g), поэтому пачка, созданная голым классом, сама находит свою строку.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AShotgunAmmoItem : public AAmmoItem
{
	GENERATED_BODY()

public:
	AShotgunAmmoItem();
};
