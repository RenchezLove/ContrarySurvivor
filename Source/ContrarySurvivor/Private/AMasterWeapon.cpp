// Fill out your copyright notice in the Description page of Project Settings.

#include "AMasterWeapon.h"

AMasterWeapon::AMasterWeapon()
{
	PrimaryActorTick.bCanEverTick = false;

	// Дефолтные значения — переопределяются в дочерних классах
	Damage             = 10.0f;
	FireRate           = 1.0f;
	Range              = 1000.0f;
	WeaponType         = EWeaponType::OneHanded;

	MaxAmmoInClip      = 10;
	CurrentAmmoInClip  = MaxAmmoInClip;
	MaxAmmoReserve     = 30;
	CurrentAmmoReserve = MaxAmmoReserve;

	bIsReloading       = false;
	LastFireTime       = -9999.0f; // Чтобы первый выстрел был доступен сразу

	// Категория для логики инвентаря/потери рюкзака (Фаза 4): оружие не теряется при смерти.
	ItemCategory = EItemCategory::Weapon;
}

void AMasterWeapon::BeginPlay()
{
	Super::BeginPlay();
}

void AMasterWeapon::Fire(AActor* Target)
{
	// Базовая реализация — переопределяется в ARangedWeapon / AMeleeWeapon
	UE_LOG(LogTemp, Warning, TEXT("AMasterWeapon::Fire() — override this in child class"));
}

void AMasterWeapon::Reload()
{
	if (!CanReload()) return;

	bIsReloading = true;

	int32 AmmoNeeded   = MaxAmmoInClip - CurrentAmmoInClip;
	int32 AmmoToAdd    = FMath::Min(AmmoNeeded, CurrentAmmoReserve);

	CurrentAmmoInClip  += AmmoToAdd;
	CurrentAmmoReserve -= AmmoToAdd;

	bIsReloading = false; // TODO: заменить на таймер когда будут анимации

	UE_LOG(LogTemp, Warning, TEXT("AMasterWeapon: Reloaded. Clip: %d / Reserve: %d"),
		CurrentAmmoInClip, CurrentAmmoReserve);
}

bool AMasterWeapon::CanFire() const
{
	if (bIsReloading)        return false;
	if (CurrentAmmoInClip <= 0) return false;

	// Проверяем кулдаун между выстрелами
	float TimeSinceLastShot = GetWorld()->GetTimeSeconds() - LastFireTime;
	if (TimeSinceLastShot < (1.0f / FireRate)) return false;

	return true;
}

bool AMasterWeapon::CanReload() const
{
	if (bIsReloading)                          return false;
	if (CurrentAmmoReserve <= 0)               return false;
	if (CurrentAmmoInClip >= MaxAmmoInClip)    return false;

	return true;
}
