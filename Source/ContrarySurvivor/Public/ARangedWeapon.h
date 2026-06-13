// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AMasterWeapon.h"
#include "ARangedWeapon.generated.h"

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API ARangedWeapon : public AMasterWeapon
{
	GENERATED_BODY()

public:
	ARangedWeapon();

protected:
	virtual void BeginPlay() override;

	// Текущая заблокированная цель (устанавливается тапом/кликом по врагу)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	AActor* LockedTarget;

	// Имя сокета на меше торса, из которого вылетает пуля
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat")
	FName MuzzleSocketName;

	// Разброс (0 = идеальная точность, 1 = максимальный разброс)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Spread;

public:

	// Выстрел по заблокированной цели
	virtual void Fire(AActor* Target) override;

	// Установить цель (вызывается из PlayerController при тапе по врагу)
	UFUNCTION(BlueprintCallable, Category = "Weapon|Combat")
	void SetTarget(AActor* NewTarget);

	// Сбросить цель
	UFUNCTION(BlueprintCallable, Category = "Weapon|Combat")
	void ClearTarget();

	// Пополнить запас патронов (резерв) на Amount, не превышая MaxAmmoReserve.
	// Используется покупкой патронов у торговца (Фаза 4, экономика, GDD §7.6).
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	void AddReserveAmmo(int32 Amount);

	// Есть ли активная цель
	UFUNCTION(BlueprintPure, Category = "Weapon|Combat")
	FORCEINLINE bool HasTarget() const { return LockedTarget != nullptr; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Combat")
	FORCEINLINE AActor* GetLockedTarget() const { return LockedTarget; }

private:

	// LineTrace от мушки до цели, возвращает true если попал
	bool PerformLineTrace(AActor* Target, FHitResult& OutHit);
};