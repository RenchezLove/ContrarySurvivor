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
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AAmmoItem : public AMasterInventoryItem
{
	GENERATED_BODY()

public:
	AAmmoItem();

	// СЧЁТЧИК СТАКА (StackCount/MaxStackCount) ПОДНЯТ В БАЗУ AMasterInventoryItem
	// (Build 1.2.1, ТЗ Г: шкура/тушёнка/аптечка стакаются «по механизму патронов»).
	// Дефолты пачки патронов (пустая пачка, лимит 999) выставляет конструктор.

	// Патроны не используются из инвентаря напрямую — заряжаются перезарядкой. Use() — no-op.
	virtual void Use() override;
};
