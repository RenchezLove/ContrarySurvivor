// Fill out your copyright notice in the Description page of Project Settings.


#include "AMasterInventoryItem.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA

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
	// Оба стакаемы + один класс + один служебный ключ. Сравнение ключа — посимвольное,
	// как в логике квестов (QuestComponent.cpp): имена задаются детерминированно.
	return Other
		&& Other != this
		&& IsStackable()
		&& Other->IsStackable()
		&& GetClass() == Other->GetClass()
		&& ItemName.Equals(Other->ItemName, ESearchCase::CaseSensitive);
}

#undef LOCTEXT_NAMESPACE