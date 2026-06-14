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

	// Сколько патронов в этой пачке (стак). Меняется покупкой (+), перезарядкой/продажей (−).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	int32 StackCount = 0;

	// Максимум патронов в одной пачке (стак-лимит). DRAFT, тюнингуется.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	int32 MaxStackCount = 999;

	UFUNCTION(BlueprintPure, Category = "Ammo")
	FORCEINLINE int32 GetStackCount() const { return StackCount; }

	UFUNCTION(BlueprintPure, Category = "Ammo")
	FORCEINLINE int32 GetStackSpace() const { return FMath::Max(0, MaxStackCount - StackCount); }

	// Патроны не используются из инвентаря напрямую — заряжаются перезарядкой. Use() — no-op.
	virtual void Use() override;
};
