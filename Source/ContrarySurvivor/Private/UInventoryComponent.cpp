// Fill out your copyright notice in the Description page of Project Settings.


#include "UInventoryComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA

// Sets default values for this component's properties
UInventoryComponent::UInventoryComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

}

// Called when the game starts
void UInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}

bool UInventoryComponent::AddItem(AMasterInventoryItem* Item)
{
    if (Item)
    {
        const int32 CountBefore = InventoryItems.Num();
        InventoryItems.Add(Item);
        const int32 CountAfter = InventoryItems.Num();
        // QA-инструментирование (BUG «лут не попадает в рюкзак»): что добавили и размер ДО/ПОСЛЕ.
        UE_LOG(LogQA, Display,
            TEXT("QA: ADDITEM '%s' (name '%s') | InventoryItems %d -> %d"),
            *Item->GetName(), *Item->ItemName, CountBefore, CountAfter);
        return true;
    }
    UE_LOG(LogQA, Display, TEXT("QA: ADDITEM called with NULL item — ignored"));
    return false;
}

void UInventoryComponent::RemoveItem(AMasterInventoryItem* Item)
{
    if (Item)
    {
        InventoryItems.Remove(Item);
        // Снятый из рюкзака предмет не может оставаться в списке экипированных.
        EquippedItems.Remove(Item);
    }
}

void UInventoryComponent::SetItemEquipped(AMasterInventoryItem* Item, bool bEquipped)
{
    if (!Item)
    {
        return;
    }

    if (bEquipped)
    {
        EquippedItems.AddUnique(Item);
    }
    else
    {
        EquippedItems.Remove(Item);
    }
}

bool UInventoryComponent::IsItemEquipped(const AMasterInventoryItem* Item) const
{
    // const_cast: Contains принимает значение того же типа; список хранит неконстантные
    // указатели. Сам предмет не модифицируется.
    return Item && EquippedItems.Contains(const_cast<AMasterInventoryItem*>(Item));
}

TArray<AMasterInventoryItem*> UInventoryComponent::GetUnequippedItemsOfCategory(EItemCategory Category) const
{
    TArray<AMasterInventoryItem*> Result;
    for (AMasterInventoryItem* Item : InventoryItems)
    {
        if (Item && Item->GetItemCategory() == Category && !IsItemEquipped(Item))
        {
            Result.Add(Item);
        }
    }
    return Result;
}

