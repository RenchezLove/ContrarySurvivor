// Fill out your copyright notice in the Description page of Project Settings.


#include "ATorsoArmor.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"

ATorsoArmor::ATorsoArmor()
{
	// ЧЕРНОВАЯ доля снижения урона слотом торса [0..1] (draft Рината: процентная броня).
	ArmorProtection = 0.25f;
	ArmorSlot = EArmorSlot::Torso;

	ItemName = FString("Torso Armor");

	// Меш-слот при экипировке (общий скелет -> Leader Pose). EquipArmor подменяет меш Torso.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Armor/SK_Armor_Torso_01.SK_Armor_Torso_01"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

void ATorsoArmor::BeginPlay()
{
	Super::BeginPlay();
}