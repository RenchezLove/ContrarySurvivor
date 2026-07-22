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
		case EConsumableType::Medkit:
			// Бинт/аптечка восстанавливает HP. Heal не лечит мёртвых (вернёт 0) — допустимо.
			Stats->Heal(HealRestoreAmount);
			return true;
		default:
			return false;
	}
}

FString AConsumableItem::GetDefaultDisplayName(EConsumableType Type)
{
	switch (Type)
	{
		case EConsumableType::Food:   return TEXT("Консервы");
		case EConsumableType::Water:  return TEXT("Вода");
		case EConsumableType::Medkit: return TEXT("Аптечка"); // решение Рината: лечащий предмет — «Аптечка»
		default:                      return TEXT("Расходник");
	}
}

FText AConsumableItem::GetDefaultDisplayText(EConsumableType Type)
{
	switch (Type)
	{
		case EConsumableType::Food:   return NSLOCTEXT("Items", "ConsumableFood", "Консервы");
		case EConsumableType::Water:  return NSLOCTEXT("Items", "ConsumableWater", "Вода");
		// Решение Рината: лечащий предмет — «Аптечка» (не «Бинт»). ВНИМАНИЕ: ключ ConsumableMedkit
		// уже в собранном переводе Content/Localization/Game/ru со значением «Бинт» — правки исходной
		// строки НЕДОСТАТОЧНО, нужен повторный сбор/компиляция локализации (см. отчёт game-lead).
		case EConsumableType::Medkit: return NSLOCTEXT("Items", "ConsumableMedkit", "Аптечка");
		default:                      return NSLOCTEXT("Items", "ConsumableGeneric", "Расходник");
	}
}
