// Fill out your copyright notice in the Description page of Project Settings.


#include "AMasterInventoryItem.h"

// Sets default values
AMasterInventoryItem::AMasterInventoryItem()
{
 	// Set this actor to call Tick() every frame.  turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false; // Items don't need to tick

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    RootComponent = ItemMesh;

}

// Called when the game starts or when spawned
void AMasterInventoryItem::BeginPlay()
{
	Super::BeginPlay();
}

void AMasterInventoryItem::Use()
{
	// Add your item-specific use logic here.  This will be overridden in child classes.
	UE_LOG(LogTemp, Warning, TEXT("AMasterInventoryItem: Use() called.  Override this function in child classes."));
}