// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Data/ContraryDataSettings.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "AMasterInventoryItem.h"
#include "AConsumableItem.h"
#include "AArmor.h"

namespace ContraryItems
{

UDataTable* GetItemTable()
{
	const UContraryDataSettings* Settings = UContraryDataSettings::Get();
	if (!Settings || Settings->ItemTable.IsNull())
	{
		return nullptr;
	}
	return Settings->ItemTable.LoadSynchronous();
}

const FContraryItemRow* FindRow(FName RowName)
{
	if (RowName.IsNone())
	{
		return nullptr;
	}
	UDataTable* Table = GetItemTable();
	if (!Table)
	{
		return nullptr;
	}
	// bWarnIfRowMissing=false: отсутствие строки — штатный откат на прежний путь, не спамим.
	return Table->FindRow<FContraryItemRow>(RowName, TEXT("ContraryItems::FindRow"), /*bWarnIfRowMissing=*/false);
}

FName FindRowNameByLegacyKey(const FString& Key)
{
	if (Key.IsEmpty())
	{
		return NAME_None;
	}
	UDataTable* Table = GetItemTable();
	if (!Table)
	{
		return NAME_None;
	}
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		const FContraryItemRow* Row = reinterpret_cast<const FContraryItemRow*>(Pair.Value);
		if (Row && Row->GetEffectiveKey(Pair.Key) == Key)
		{
			return Pair.Key;
		}
	}
	return NAME_None;
}

void ApplyRowToItem(AMasterInventoryItem& Item, const FContraryItemRow& Row, FName RowName, int32 StackCount)
{
	// Идентичность: служебный ключ (контракт ADR-050 — тот же ключ, что раньше задавали
	// конструкторы/спавнеры), переводимое название, категория.
	Item.ItemName = Row.GetEffectiveKey(RowName);
	// Происхождение: из какой строки собран экземпляр. Нужно сейву — при «Продолжить» строка
	// накладывается на восстановленный предмет заново (класс идентичность не хранит).
	Item.SourceItemRow = RowName;
	if (!Row.DisplayText.IsEmpty())
	{
		Item.ItemDisplayText = Row.DisplayText;
	}
	Item.ItemCategory = Row.Category;

	// Витрина: иконка и меш в мире (пусто в строке = остаются значения класса/BP).
	if (!Row.Icon.IsNull())
	{
		Item.ItemIcon = Row.Icon;
	}
	if (!Row.WorldMesh.IsNull() && Item.ItemMesh)
	{
		if (UStaticMesh* Mesh = Row.WorldMesh.LoadSynchronous())
		{
			Item.ItemMesh->SetStaticMesh(Mesh);
		}
	}

	// Стак: лимит из строки (0 = как у класса), затем сам счётчик с обрезкой по лимиту.
	if (Row.MaxStackCount > 0)
	{
		Item.MaxStackCount = Row.MaxStackCount;
	}
	if (StackCount > 0)
	{
		Item.StackCount = FMath::Clamp(StackCount, 1, FMath::Max(1, Item.MaxStackCount));
	}

	// Расходник: тип (еда/вода/аптечка). Величины восстановления — поведение, живут на
	// классе и UStatsComponent, из таблицы не трогаются (анти-дубль, спека §0).
	if (AConsumableItem* Consumable = Cast<AConsumableItem>(&Item))
	{
		Consumable->ConsumableType = Row.ConsumableType;
	}

	// Броня: слот, защита (0 = как у класса), слотовый меш экипировки.
	if (AArmor* Armor = Cast<AArmor>(&Item))
	{
		Armor->ArmorSlot = Row.ArmorSlot;
		if (Row.ArmorProtection > 0.0f)
		{
			Armor->ArmorProtection = Row.ArmorProtection;
		}
		if (!Row.EquipMesh.IsNull())
		{
			if (USkeletalMesh* EquipMesh = Row.EquipMesh.LoadSynchronous())
			{
				Armor->ArmorMesh_Equipped = EquipMesh;
			}
		}
	}
}

AMasterInventoryItem* SpawnItemFromRow(UWorld* World, FName RowName, int32 StackCount, const FTransform& Transform)
{
	if (!World)
	{
		return nullptr;
	}
	const FContraryItemRow* Row = FindRow(RowName);
	if (!Row)
	{
		return nullptr;
	}
	UClass* ItemClass = Row->ItemClass.LoadSynchronous();
	if (!ItemClass)
	{
		// Строка есть, а класса нет — это уже ошибка данных, о ней сказать (один раз на вызов).
		UE_LOG(LogTemp, Warning,
			TEXT("ContraryItems::SpawnItemFromRow: у строки '%s' таблицы предметов не задан или не загрузился класс актора — предмет не создан."),
			*RowName.ToString());
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AMasterInventoryItem* Item = World->SpawnActor<AMasterInventoryItem>(ItemClass, Transform, SpawnParams);
	if (!Item)
	{
		return nullptr;
	}
	ApplyRowToItem(*Item, *Row, RowName, StackCount);
	return Item;
}

} // namespace ContraryItems
