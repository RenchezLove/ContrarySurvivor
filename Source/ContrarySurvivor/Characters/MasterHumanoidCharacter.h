// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AMasterWeapon.h"

class UInventoryComponent;
class AArmor;

#include "MasterHumanoidCharacter.generated.h"

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API AMasterHumanoidCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMasterHumanoidCharacter();

	UFUNCTION(BlueprintCallable)
    void SetSprint(bool bIsSprinting);

protected:
	virtual void BeginPlay() override;

	// --- Статы ---

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float Health;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
    float MaxHealth;

	// --- Состояние боя ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    bool bIsAttacking;

	// --- Меши ---
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* HeadMesh; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* TorsoMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* LegsMesh; 

	// --- Компоненты ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UInventoryComponent* Inventory;

	// --- Оружие ---

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Equipment")
	AMasterWeapon* CurrentWeapon;

	// Сокет на TorsoMesh к которому крепится оружие
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	FName WeaponSocketName;

	// --- Экипированная броня по слотам (GDD §7.2: броня влияет на урон) ---
	// Хранятся ссылки на экипированные предметы брони; суммарная защита снижает
	// входящий урон в TakeDamage. Полноценная экип-UI — Фаза 4.

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment|Armor")
	AArmor* EquippedHeadArmor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment|Armor")
	AArmor* EquippedTorsoArmor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment|Armor")
	AArmor* EquippedPantsArmor;

public:
	virtual void Tick(float DeltaTime) override;

	// --- Функции оружия ---

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void EquipWeapon(AMasterWeapon* NewWeapon);

	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UnequipWeapon();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void FireCurrentWeapon(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ReloadCurrentWeapon();

	// --- Броня ---

	// Экипирует предмет брони в слот по его типу (Head/Torso/Pants). Хранит ссылку для
	// расчёта защиты. Визуальное назначение меша брони — Фаза 4 (здесь только параметры).
	UFUNCTION(BlueprintCallable, Category = "Equipment|Armor")
	void EquipArmor(AArmor* Armor);

	// Суммарная защита всех экипированных слотов брони (flat). Читается в TakeDamage.
	UFUNCTION(BlueprintPure, Category = "Equipment|Armor")
	float GetTotalArmorProtection() const;

	// --- Геттеры ---

	UFUNCTION(BlueprintCallable, Category = "Components")
	FORCEINLINE USkeletalMeshComponent* GetTorsoMesh() const { return TorsoMesh; }

	UFUNCTION(BlueprintPure, Category = "Equipment")
	FORCEINLINE AMasterWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	UFUNCTION(BlueprintCallable, Category = "Stats")
	FORCEINLINE float GetHealth() const { return Health; }

	UFUNCTION(BlueprintCallable, Category = "Stats")
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	// --- Внешний вид ---

	UFUNCTION(BlueprintCallable, Category = "Appearance")
	void UpdateCharacterAppearance();

	// --- Урон и лечение ---

	UFUNCTION(BlueprintCallable, Category = "Stats")
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	virtual void RestoreHealth(float HealAmount);

	// Вызывается при достижении 0 HP (инлайн-Health база). Минимальная заглушка;
	// дочерние классы могут переопределить (враг использует UStatsComponent вместо этого).
	UFUNCTION(BlueprintCallable, Category = "Stats")
	virtual void HandleDeath();

private:
    float BaseWalkSpeed;
    float SprintMultiplier = 2.0f;
    bool IsSprinting;	
};
