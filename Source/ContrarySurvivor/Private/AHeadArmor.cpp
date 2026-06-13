// Fill out your copyright notice in the Description page of Project Settings.


#include "AHeadArmor.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"

AHeadArmor::AHeadArmor()
{
	// ЧЕРНОВАЯ доля снижения урона слотом головы [0..1] (draft Рината: процентная броня).
	ArmorProtection = 0.10f;
	ArmorSlot = EArmorSlot::Head;

	ItemName = FString("Head Armor");

	// Меш-слот при экипировке (на ОБЩЕМ скелете персонажа -> Leader Pose даёт синхронную
	// анимацию). EquipArmor подменяет меш слота Head на этот ассет.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Armor/SK_Armor_Head_01.SK_Armor_Head_01"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

void AHeadArmor::BeginPlay()
{
	Super::BeginPlay();
}