// Fill out your copyright notice in the Description page of Project Settings.

#include "AConsumableItem.h"
#include "ContrarySurvivor/Components/StatsComponent.h"

AConsumableItem::AConsumableItem()
{
	// Категория расходника (база ставит Resource) — для логики UI/потери при смерти.
	ItemCategory = EItemCategory::Consumable;

	// Build 1.2.1 (ТЗ Г): расходники СТАКАЮТСЯ по механизму патронов, лимит как у патронов
	// (999). Один класс на воду/консервы/аптечку — различает стаки служебный ключ ItemName
	// (CanStackWith). Слияние — UInventoryComponent::AddItem.
	StackCount = 1;
	MaxStackCount = 999;
}

bool AConsumableItem::ApplyConsumeEffect(UStatsComponent* Stats)
{
	if (!Stats)
	{
		return false;
	}

	bool bApplied = false;
	switch (ConsumableType)
	{
		case EConsumableType::Food:
			Stats->ConsumeFood();   // +FoodRestoreAmount к голоду (Фаза 2)
			bApplied = true;
			break;
		case EConsumableType::Water:
			Stats->DrinkWater();    // +WaterRestoreAmount к жажде (Фаза 2)
			bApplied = true;
			break;
		case EConsumableType::Medkit:
			// Бинт/аптечка восстанавливает HP. Heal не лечит мёртвых (вернёт 0) — допустимо.
			Stats->Heal(HealRestoreAmount);
			bApplied = true;
			break;
		default:
			break;
	}

	// Build 1.2.1 (ТЗ Г): из СТАКА съедается одна штука. Возвращаемое значение читается
	// вызывающим (APlayerCharacter::Inv_UseBackpackItem) как «предмет израсходован ЦЕЛИКОМ —
	// убрать из рюкзака и уничтожить»: пока в стаке осталось >0 штук, отвечаем false и лишь
	// уменьшаем счётчик; последняя штука — прежнее поведение (true -> актор уничтожается).
	if (bApplied && StackCount > 1)
	{
		--StackCount;
		return false;
	}
	return bApplied;
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

TSoftObjectPtr<UTexture2D> AConsumableItem::GetDefaultIcon(EConsumableType Type)
{
	// Рендеры модельера (Build 1.2.2, тайлы) — мягкие ссылки: текстуры может не быть
	// в копии проекта, UI обязан жить без неё (тайл покажет только подпись).
	const TCHAR* Path = nullptr;
	switch (Type)
	{
		case EConsumableType::Food:   Path = TEXT("/Game/UI/Icons/Items/T_Item_CannedFood.T_Item_CannedFood"); break;
		case EConsumableType::Water:  Path = TEXT("/Game/UI/Icons/Items/T_Item_Water.T_Item_Water"); break;
		case EConsumableType::Medkit: Path = TEXT("/Game/UI/Icons/Items/T_Item_Medkit.T_Item_Medkit"); break;
		default: return TSoftObjectPtr<UTexture2D>();
	}
	return TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Path));
}

TSoftObjectPtr<UTexture2D> AConsumableItem::GetItemIcon() const
{
	// Явно заданная иконка (экземпляр/BP) главнее вычисленной по типу.
	return ItemIcon.IsNull() ? GetDefaultIcon(ConsumableType) : ItemIcon;
}
