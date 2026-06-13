// Fill out your copyright notice in the Description page of Project Settings.


#include "AArmor.h"

AArmor::AArmor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false; // No tick needed for Armor
    ArmorProtection = 0.0f; // Set default armor rating

    // Категория для логики инвентаря/потери рюкзака (Фаза 4): броня не теряется при смерти.
    ItemCategory = EItemCategory::Armor;

    // Дефолт-слот (наследники переопределяют): Torso как нейтральный.
    ArmorSlot = EArmorSlot::Torso;
}

void AArmor::BeginPlay()
{
	Super::BeginPlay();
	// You can load mesh here if you wish
}

