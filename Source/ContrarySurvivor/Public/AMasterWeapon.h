// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AMasterInventoryItem.h"
#include "AMasterWeapon.generated.h"

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	OneHanded    UMETA(DisplayName = "One Handed"),   // Пистолет, нож
	TwoHanded    UMETA(DisplayName = "Two Handed")    // Дробовик
};

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API AMasterWeapon : public AMasterInventoryItem
{
	GENERATED_BODY()

public:
	AMasterWeapon();

protected:
	virtual void BeginPlay() override;

	// --- Параметры оружия ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats")
	float Damage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats")
	float FireRate;           // Выстрелов в секунду

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats")
	float Range;              // Дальность (в Unreal units)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Stats")
	EWeaponType WeaponType;

	// --- Патроны ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 MaxAmmoInClip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 CurrentAmmoInClip;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 MaxAmmoReserve;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 CurrentAmmoReserve;

	// --- Состояние ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|State")
	bool bIsReloading;

	// Таймер между выстрелами
	float LastFireTime;

public:

	// --- Основные функции ---

	// Выстрел — переопределяется в дочерних классах
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Fire(AActor* Target);

	// Перезарядка
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	virtual void Reload();

	// Можно ли стрелять прямо сейчас
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool CanFire() const;

	// Есть ли патроны для перезарядки
	UFUNCTION(BlueprintPure, Category = "Weapon")
	bool CanReload() const;

	// --- Геттеры ---

	UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
	FORCEINLINE float GetDamage() const { return Damage; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Stats")
	FORCEINLINE EWeaponType GetWeaponType() const { return WeaponType; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	FORCEINLINE int32 GetCurrentAmmoInClip() const { return CurrentAmmoInClip; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
	FORCEINLINE int32 GetCurrentAmmoReserve() const { return CurrentAmmoReserve; }

	UFUNCTION(BlueprintPure, Category = "Weapon|State")
	FORCEINLINE bool GetIsReloading() const { return bIsReloading; }
};
