// Fill out your copyright notice in the Description page of Project Settings.


#include "UInventoryComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Debug/QADebug.h"    // QA-хелпер (оверлей/flush)

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
        // Build 1.2.1 (ТЗ Г): СЛИЯНИЕ СТАКОВ — единая точка на все пути пополнения рюкзака
        // (подбор пикапа, покупка, обыск трупа, отладочная выдача). Стакаемый входящий
        // предмет доливается в существующие стаки того же класса/ключа, пока есть место;
        // влившийся ЦЕЛИКОМ актор уничтожается (Destroy отложенный — указатель у вызывающего
        // до конца кадра валиден), остаток входит отдельной записью. Экипированное не
        // трогаем (стакаемое не экипируется, guard на всякий случай).
        if (Item->IsStackable() && Item->GetStackCount() > 0)
        {
            for (AMasterInventoryItem* Existing : InventoryItems)
            {
                if (!IsValid(Existing) || !Existing->CanStackWith(Item) || IsItemEquipped(Existing))
                {
                    continue;
                }
                const int32 Transfer = FMath::Min(Existing->GetStackSpace(), Item->StackCount);
                if (Transfer <= 0)
                {
                    continue;
                }
                Existing->StackCount += Transfer;
                Item->StackCount -= Transfer;
                FQADebug::QA(this, FString::Printf(
                    TEXT("QA: ADDITEM merge %d x '%s' -> stack %d/%d"),
                    Transfer, *Item->ItemName, Existing->StackCount, Existing->MaxStackCount),
                    /*bScreen=*/true);
                if (Item->StackCount <= 0)
                {
                    Item->Destroy();
                    return true; // всё влилось в существующие стаки — новой записи нет
                }
            }
        }

        const int32 CountBefore = InventoryItems.Num();
        InventoryItems.Add(Item);
        const int32 CountAfter = InventoryItems.Num();
        // QA-инструментирование (BUG «лут не попадает в рюкзак»): имя предмета + размер инвентаря ДО->ПОСЛЕ.
        const FString DisplayName = Item->ItemName.IsEmpty() ? Item->GetName() : Item->ItemName;
        FQADebug::QA(this, FString::Printf(
            TEXT("QA: ADDITEM %s -> inv %d->%d"), *DisplayName, CountBefore, CountAfter), /*bScreen=*/true);
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

