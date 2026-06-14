// Fill out your copyright notice in the Description page of Project Settings.

#include "APistol.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APistol::APistol()
{
	// Реальный меш пистолета (Демо). ItemMesh — корневой StaticMeshComponent базы
	// AMasterInventoryItem (как у ножа в AMeleeWeapon). Материал уже на ассете.
	// BP-наследник может переопределить.
	if (ItemMesh)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> PistolMeshAsset(
			TEXT("/Game/Meshes/Demo/SM_Pistol.SM_Pistol"));
		if (PistolMeshAsset.Succeeded())
		{
			ItemMesh->SetStaticMesh(PistolMeshAsset.Object);
		}
		// Пистолет носится прикреплённым к персонажу; собственная коллизия не нужна
		// (как у ножа) — иначе мешает капсуле/трейсам.
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Параметры пистолета
	Damage             = 25.0f;
	FireRate           = 2.0f;   // 2 выстрела в секунду
	Range              = 1500.0f;
	WeaponType         = EWeaponType::OneHanded;
	Spread             = 0.03f;  // Небольшой разброс

	// Патроны
	MaxAmmoInClip      = 12;
	CurrentAmmoInClip  = MaxAmmoInClip;
	MaxAmmoReserve     = 48;
	CurrentAmmoReserve = MaxAmmoReserve;

	// Имя предмета (для инвентаря)
	ItemName        = FString("Pistol");
	ItemDescription = FString("A reliable sidearm. 12 rounds per magazine.");

	MuzzleSocketName = FName("MuzzleSocket");
}
