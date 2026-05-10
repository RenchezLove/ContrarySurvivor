// Fill out your copyright notice in the Description page of Project Settings.

#include "APistol.h"

APistol::APistol()
{
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
