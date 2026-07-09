// Fill out your copyright notice in the Description page of Project Settings.


#include "AArmorTiers.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"

// Черновые доли снижения урона на слот (решение Рината 07-07): Т1=0.05 / Т2=0.10 / Т3=0.16.
// FObjectFinder в каждом конструкторе свой (static кэшируется по месту вызова —
// общий хелпер закэшировал бы первый путь на все девять классов).

// ---------------- Т1 ----------------

AHeadArmorT1::AHeadArmorT1()
{
	ArmorProtection = 0.05f;
	ArmorSlot = EArmorSlot::Head;
	ItemName = TEXT("Броня Т1 — голова");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Shared/Armor/SK_Armor_T1_Head.SK_Armor_T1_Head"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

ATorsoArmorT1::ATorsoArmorT1()
{
	ArmorProtection = 0.05f;
	ArmorSlot = EArmorSlot::Torso;
	ItemName = TEXT("Броня Т1 — торс");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Shared/Armor/SK_Armor_T1_Torso.SK_Armor_T1_Torso"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

APantsArmorT1::APantsArmorT1()
{
	ArmorProtection = 0.05f;
	ArmorSlot = EArmorSlot::Legs;
	ItemName = TEXT("Броня Т1 — штаны");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Shared/Armor/SK_Armor_T1_Legs.SK_Armor_T1_Legs"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

// ---------------- Т2 ----------------

AHeadArmorT2::AHeadArmorT2()
{
	ArmorProtection = 0.10f;
	ArmorSlot = EArmorSlot::Head;
	ItemName = TEXT("Броня Т2 — голова");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Shared/Armor/SK_Armor_T2_Head.SK_Armor_T2_Head"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

ATorsoArmorT2::ATorsoArmorT2()
{
	ArmorProtection = 0.10f;
	ArmorSlot = EArmorSlot::Torso;
	ItemName = TEXT("Броня Т2 — торс");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Shared/Armor/SK_Armor_T2_Torso.SK_Armor_T2_Torso"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

APantsArmorT2::APantsArmorT2()
{
	ArmorProtection = 0.10f;
	ArmorSlot = EArmorSlot::Legs;
	ItemName = TEXT("Броня Т2 — штаны");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Shared/Armor/SK_Armor_T2_Legs.SK_Armor_T2_Legs"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

// ---------------- Т3 ----------------

AHeadArmorT3::AHeadArmorT3()
{
	ArmorProtection = 0.16f;
	ArmorSlot = EArmorSlot::Head;
	ItemName = TEXT("Броня Т3 — голова");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Shared/Armor/SK_Armor_T3_Head.SK_Armor_T3_Head"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

ATorsoArmorT3::ATorsoArmorT3()
{
	ArmorProtection = 0.16f;
	ArmorSlot = EArmorSlot::Torso;
	ItemName = TEXT("Броня Т3 — торс");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Shared/Armor/SK_Armor_T3_Torso.SK_Armor_T3_Torso"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}

APantsArmorT3::APantsArmorT3()
{
	ArmorProtection = 0.16f;
	ArmorSlot = EArmorSlot::Legs;
	ItemName = TEXT("Броня Т3 — штаны");

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmorMeshFinder(
		TEXT("/Game/Characters/Shared/Armor/SK_Armor_T3_Legs.SK_Armor_T3_Legs"));
	if (ArmorMeshFinder.Succeeded())
	{
		ArmorMesh_Equipped = ArmorMeshFinder.Object;
	}
}
