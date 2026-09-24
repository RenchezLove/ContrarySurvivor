// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "ContrarySurvivor/Data/ContraryDataSettings.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
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

	// Витрина. Иконка — ВСЕГДА из строки (ADR-088): таблица — единственный источник картинки,
	// картинка чертежа или класса её больше не перебивает. Пустая ячейка = предмет без
	// картинки (плитка покажет подпись) — это сигнал заполнить таблицу, а не молчаливый откат.
	Item.ItemIcon = Row.Icon;
	// Меш в мире: пусто в строке = остаётся меш класса/BP.
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

AMasterInventoryItem* SpawnItem(UWorld* World, UClass* ItemClass, const FTransform& Transform,
	AActor* Owner, const TFunction<void(AMasterInventoryItem&)>& Init)
{
	if (!World || !ItemClass || !ItemClass->IsChildOf(AMasterInventoryItem::StaticClass()))
	{
		return nullptr;
	}
	AMasterInventoryItem* Item = World->SpawnActorDeferred<AMasterInventoryItem>(ItemClass, Transform, Owner,
		/*Instigator=*/nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Item)
	{
		return nullptr;
	}
	if (Init)
	{
		Init(*Item);
	}
	// Здесь движок вызывает PostInitializeComponents — предмет накладывает свою строку.
	UGameplayStatics::FinishSpawningActor(Item, Transform);
	return IsValid(Item) ? Item : nullptr;
}

AMasterInventoryItem* SpawnItemFromRow(UWorld* World, FName RowName, int32 StackCount, const FTransform& Transform,
	AActor* Owner)
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

	// Строку накладывает сам предмет при появлении (единая точка) — здесь только передаём её.
	return SpawnItem(World, ItemClass, Transform, Owner, [RowName, StackCount](AMasterInventoryItem& Item)
	{
		Item.SourceItemRow = RowName;
		if (StackCount > 0)
		{
			Item.StackCount = StackCount; // обрежется лимитом строки при наложении
		}
	});
}

} // namespace ContraryItems
