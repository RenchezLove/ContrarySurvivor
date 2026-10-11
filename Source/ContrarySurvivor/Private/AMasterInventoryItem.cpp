// Fill out your copyright notice in the Description page of Project Settings.


#include "AMasterInventoryItem.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "InventoryItem"

// Sets default values
AMasterInventoryItem::AMasterInventoryItem()
{
 	// Set this actor to call Tick() every frame.  turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false; // Items don't need to tick

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    RootComponent = ItemMesh;

    // База — нейтральная категория Resource. Наследники переопределяют в своих
    // конструкторах (AArmor -> Armor, AMasterWeapon -> Weapon).
    ItemCategory = EItemCategory::Resource;
}

// Called when the game starts or when spawned
void AMasterInventoryItem::BeginPlay()
{
	Super::BeginPlay();
}

void AMasterInventoryItem::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Только в игровом мире (игра, PIE, автотесты): в редакторе и в превью чертежа данные
	// строки не должны записываться в экземпляр на карте и сохраняться вместе с уровнем.
	const UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		ApplyOwnItemRow();
	}
}

FName AMasterInventoryItem::ResolveItemRowName() const
{
	if (!SourceItemRow.IsNone() && ContraryItems::FindRow(SourceItemRow))
	{
		return SourceItemRow;
	}
	return ContraryItems::FindRowNameByLegacyKey(ItemName);
}

void AMasterInventoryItem::ApplyOwnItemRow()
{
	if (bItemRowResolved)
	{
		return; // строка накладывается один раз — повторный вызов не должен перетирать данные
	}
	bItemRowResolved = true;

	const FName RowName = ResolveItemRowName();
	if (const FContraryItemRow* Row = ContraryItems::FindRow(RowName))
	{
		// StackCount спавнер выставил ДО этой точки — здесь он лишь обрезается лимитом строки.
		ContraryItems::ApplyRowToItem(*this, *Row, RowName, StackCount);
	}
	else if (!SourceItemRow.IsNone() && ContraryItems::GetItemTable())
	{
		// Строку назначили явно, а в таблице её нет — ошибка данных, молчать нельзя
		// (таблица не назначена вовсе — штатный режим без таблиц, не спамим).
		UE_LOG(LogQA, Warning,
			TEXT("AMasterInventoryItem '%s': строка '%s' не найдена в таблице предметов — остаются значения класса"),
			*GetName(), *SourceItemRow.ToString());
	}

	// Вне таблицы лимит стака остаётся классовым — счётчик не должен его превышать.
	if (StackCount > MaxStackCount)
	{
		StackCount = FMath::Max(1, MaxStackCount);
	}
}

TArray<FName> AMasterInventoryItem::GetItemRowOptions() const
{
	TArray<FName> Options;
	Options.Add(NAME_None); // пусто = строка по служебному ключу
	if (const UDataTable* Table = ContraryItems::GetItemTable())
	{
		Options.Append(Table->GetRowNames());
	}
	return Options;
}

void AMasterInventoryItem::Use()
{
	// Add your item-specific use logic here.  This will be overridden in child classes.
	UE_LOG(LogTemp, Warning, TEXT("AMasterInventoryItem: Use() called.  Override this function in child classes."));
}

FText AMasterInventoryItem::GetItemDisplayText() const
{
	// 1. Переводимое название, если его заполнил тот, кто создавал предмет.
	if (!ItemDisplayText.IsEmpty())
	{
		return ItemDisplayText;
	}

	// 2. Служебный ключ как есть — ровно прежнее поведение, с экрана ничего не пропадает.
	if (!ItemName.IsEmpty())
	{
		return FText::FromString(ItemName);
	}

	// 3. Не заполнено ни то, ни другое: раньше сюда подставлялся GetName() и игрок видел
	// «BP_Pistol_C_1» (ADR-049). Показываем нейтральное слово, служебное имя уводим в лог.
	UE_LOG(LogQA, Warning,
		TEXT("AMasterInventoryItem '%s': не заданы ни ItemDisplayText, ни ItemName — игроку показана заглушка"),
		*GetName());
	return LOCTEXT("UnnamedItem", "Предмет");
}

bool AMasterInventoryItem::CanStackWith(const AMasterInventoryItem* Other) const
{
	if (!Other || Other == this || !IsStackable() || !Other->IsStackable())
	{
		return false;
	}

	// Один и тот же предмет — это одна и та же строка таблицы предметов, а не один и тот же
	// класс (ADR-075). Корень раздвоенных стопок 11.10.2026 («одна плитка с 36 патронами 9мм и
	// другая… 10 шт»): патроны купленного ствола ложились пачкой класса из кода (AAmmoItem), а
	// патроны с трупа — пачкой класса из таблицы (BP_Ammo9mm); строка у них одна, классы
	// разные, и прежнее сравнение по классу стопки не сливало. То же было у подаренной и
	// найденной аптечки.
	const FName MyRow = ResolveItemRowName();
	const FName OtherRow = Other->ResolveItemRowName();
	if (!MyRow.IsNone() || !OtherRow.IsNone())
	{
		return MyRow == OtherRow;
	}

	// Предметов нет в таблице (игра без таблиц, проверочные предметы) — прежнее правило:
	// один класс и один служебный ключ, сравнение ключа посимвольное.
	return GetClass() == Other->GetClass()
		&& ItemName.Equals(Other->ItemName, ESearchCase::CaseSensitive);
}

#undef LOCTEXT_NAMESPACE