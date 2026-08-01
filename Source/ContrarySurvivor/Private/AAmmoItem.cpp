// Fill out your copyright notice in the Description page of Project Settings.

#include "AAmmoItem.h"

AAmmoItem::AAmmoItem()
{
	// Имя по умолчанию + категория Resource (запас): частично теряется при смерти (GDD §7.8).
	ItemName = TEXT("Патроны 9мм");
	ItemDisplayText = NSLOCTEXT("Items", "Ammo9mm", "Патроны 9мм");
	ItemCategory = EItemCategory::Resource;

	// Стак (поля в базе с Build 1.2.1): пачка создаётся ПУСТОЙ (счётчик наполняет
	// покупка/размещённый пикап — прежнее поведение AAmmoItem), лимит 999 (DRAFT).
	StackCount = 0;
	MaxStackCount = 999;
}

void AAmmoItem::Use()
{
	// Патроны не используются из инвентаря: заряжаются в оружие штатной перезарядкой (R).
	// No-op (намеренно), чтобы клик «Use» по пачке в инвентаре ничего не ломал.
}
