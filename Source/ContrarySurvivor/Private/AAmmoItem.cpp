// Fill out your copyright notice in the Description page of Project Settings.

#include "AAmmoItem.h"

AAmmoItem::AAmmoItem()
{
	// Имя по умолчанию + категория Resource (запас): частично теряется при смерти (GDD §7.8).
	ItemName = TEXT("Патроны 9мм");
	ItemDisplayText = NSLOCTEXT("Items", "Ammo9mm", "Патроны 9мм");
	ItemCategory = EItemCategory::Resource;

	// Картинка пачки — только из строки таблицы DT_Items (ADR-088 п.2), здесь не задаётся.

	// Стак (поля в базе с Build 1.2.1): пачка создаётся ПУСТОЙ (счётчик наполняет
	// покупка/размещённый пикап — прежнее поведение AAmmoItem), лимит 999 (DRAFT).
	StackCount = 0;
	MaxStackCount = 999;

	// Вид патрона класса — прежний пистолетный (двустволка, 09.10.2026).
	AmmoType = ContraryAmmo::DefaultType();
}

FName AAmmoItem::GetAmmoType() const
{
	// Строка таблицы главнее класса: один класс пачки может обслуживать разные калибры,
	// различает их строка (тот же принцип, что у брони и расходников, ADR-075).
	return SourceItemRow.IsNone() ? ContraryAmmo::Normalize(AmmoType) : SourceItemRow;
}

AShotgunAmmoItem::AShotgunAmmoItem()
{
	// Ключ = имя строки таблицы (путь НОВЫХ предметов: колонка старого ключа пуста).
	ItemName = ContraryAmmo::Shotgun12gType().ToString();
	ItemDisplayText = NSLOCTEXT("Items", "Ammo12g", "Патроны 12 калибра");
	AmmoType = ContraryAmmo::Shotgun12gType();
}

void AAmmoItem::Use()
{
	// Патроны не используются из инвентаря: заряжаются в оружие штатной перезарядкой (R).
	// No-op (намеренно), чтобы клик «Use» по пачке в инвентаре ничего не ломал.
}
