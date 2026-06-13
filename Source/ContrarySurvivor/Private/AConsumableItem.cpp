// Fill out your copyright notice in the Description page of Project Settings.

#include "AConsumableItem.h"
#include "ContrarySurvivor/Components/StatsComponent.h"

AConsumableItem::AConsumableItem()
{
	// Категория расходника (база ставит Resource) — для логики UI/потери при смерти.
	ItemCategory = EItemCategory::Consumable;
}

bool AConsumableItem::ApplyConsumeEffect(UStatsComponent* Stats)
{
	if (!Stats)
	{
		return false;
	}

	switch (ConsumableType)
	{
		case EConsumableType::Food:
			Stats->ConsumeFood();   // +FoodRestoreAmount к голоду (Фаза 2)
			return true;
		case EConsumableType::Water:
			Stats->DrinkWater();    // +WaterRestoreAmount к жажде (Фаза 2)
			return true;
		default:
			return false;
	}
}
