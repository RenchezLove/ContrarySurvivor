// Fill out your copyright notice in the Description page of Project Settings.


#include "UInventoryComponent.h"

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
        InventoryItems.Add(Item);
        return true;
    }
    return false;
}

void UInventoryComponent::RemoveItem(AMasterInventoryItem* Item)
{
    if (Item)
    {
        InventoryItems.Remove(Item);
    }
}

