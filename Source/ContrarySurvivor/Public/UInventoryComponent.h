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

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (AllowPrivateAccess = "true"))
    TArray<AMasterInventoryItem*> InventoryItems;

};