// Fill out your copyright notice in the Description page of Project Settings.

// UInventoryComponent.h

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AMasterInventoryItem.h" 
#include "UInventoryComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UInventoryComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Functions
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddItem(AMasterInventoryItem* Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void RemoveItem(AMasterInventoryItem* Item);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	FORCEINLINE TArray<AMasterInventoryItem*> GetInventoryItems() const { return InventoryItems; }

	// --- Отслеживание экипировки (Фаза 4) ---
	// Помечает предмет как экипированный/снятый. Экипированные предметы не теряются
	// при смерти (GDD §7.8). Предмет может лежать в InventoryItems и быть экипированным
	// одновременно (надетая броня/оружие в руках).
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void SetItemEquipped(AMasterInventoryItem* Item, bool bEquipped);

	UFUNCTION(BlueprintPure, Category = "Inventory")
	bool IsItemEquipped(const AMasterInventoryItem* Item) const;

	// Предметы инвентаря выбранной категории, НЕ помеченные экипированными.
	// Используется потерей рюкзака при смерти (расходники/ресурсы) и будущим UI.
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	TArray<AMasterInventoryItem*> GetUnequippedItemsOfCategory(EItemCategory Category) const;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
    TArray<AMasterInventoryItem*> InventoryItems;

    // Подмножество предметов, помеченных экипированными (надетая броня, оружие в руках).
    // Хранится отдельным списком, чтобы предмет мог одновременно быть в рюкзаке и надетым.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
    TArray<AMasterInventoryItem*> EquippedItems;

};