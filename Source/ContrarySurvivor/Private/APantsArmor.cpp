// Fill out your copyright notice in the Description page of Project Settings.


#include "APantsArmor.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"

APantsArmor::APantsArmor()
{
	// ЧЕРНОВАЯ доля снижения урона слотом штанов [0..1] (draft Рината: процентная броня).
	ArmorProtection = 0.15f;
	ArmorSlot = EArmorSlot::Legs;

	ItemName = FString("Pants Armor");

	// Меш-слот при экипировке (общий скелет -> Leader Pose). EquipArmor подменяет меш Legs.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Armor/SK_Armor_Legs_01.SK_Armor_Legs_01"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

void APantsArmor::BeginPlay()
{
	Super::BeginPlay();
}