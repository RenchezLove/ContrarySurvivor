// Fill out your copyright notice in the Description page of Project Settings.

#include "AAmmoItem.h"

AAmmoItem::AAmmoItem()
{
	// Имя по умолчанию + категория Resource (запас): частично теряется при смерти (GDD §7.8).
	ItemName = TEXT("Патроны");
	ItemCategory = EItemCategory::Resource;
}

void AAmmoItem::Use()
{
	// Патроны не используются из инвентаря: заряжаются в оружие штатной перезарядкой (R).
	// No-op (намеренно), чтобы клик «Use» по пачке в инвентаре ничего не ломал.
}
