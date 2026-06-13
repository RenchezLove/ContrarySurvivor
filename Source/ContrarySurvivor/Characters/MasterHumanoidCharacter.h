// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AMasterWeapon.h"
#include "AArmor.h" // AArmor + EArmorSlot (тип параметра UFUNCTION UnequipArmor)

class UInventoryComponent;

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

	// Потолок суммарного процентного снижения урона бронёй [0..1] (решение Рината:
	// процентная броня). Final = Incoming * (1 - clamp(SumArmorFraction, 0, Cap)).
	// DRAFT = 0.75 (макс −75% урона, всегда остаётся минимум 25% — нет min-1 неуязвимости).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment|Armor", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArmorReductionCap;

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

	// Экипирует предмет брони в слот по его GetArmorSlot() (Head/Torso/Legs). Хранит ссылку
	// для расчёта защиты (GetTotalArmorProtection) И подменяет модульный скелетный меш
	// соответствующего слота на Armor->GetMesh() (ArmorMesh_Equipped) (GDD §7.4). Если в
	// слоте уже была броня — она снимается (меш слота возвращается к базовому перед сменой).
	UFUNCTION(BlueprintCallable, Category = "Equipment|Armor")
	void EquipArmor(AArmor* Armor);

	// Снимает броню из слота: очищает ссылку (защита пересчитывается) и возвращает
	// модульный меш слота к базовому (запомненному в BeginPlay). Меш слота снова
	// анимируется синхронно с телом через Leader Pose.
	UFUNCTION(BlueprintCallable, Category = "Equipment|Armor")
	void UnequipArmor(EArmorSlot Slot);

	// Суммарная ДОЛЯ снижения урона по всем экипированным слотам брони [0..N] (без капа;
	// кап применяется в ComputeArmoredDamage). Читается при расчёте урона.
	UFUNCTION(BlueprintPure, Category = "Equipment|Armor")
	float GetTotalArmorProtection() const;

	// Применяет процентную броню к входящему урону (решение Рината):
	// Final = Incoming * (1 - clamp(GetTotalArmorProtection(), 0, ArmorReductionCap)).
	// Используется в TakeDamage игрока и врага. Процент всегда оставляет часть урона —
	// убирает min-1 неуязвимость старой flat-формулы.
	UFUNCTION(BlueprintPure, Category = "Equipment|Armor")
	float ComputeArmoredDamage(float Incoming) const;

	// Возвращает экипированную броню в слоте (или nullptr). Для сохранения/UI.
	UFUNCTION(BlueprintPure, Category = "Equipment|Armor")
	AArmor* GetEquippedArmor(EArmorSlot Slot) const;

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

protected:
	// Модульный меш-компонент слота (Head=GetMesh()/Torso/Legs). nullptr для неизвестного.
	USkeletalMeshComponent* GetMeshComponentForSlot(EArmorSlot Slot) const;

	// Перепривязывает меш-компонент слота к Leader Pose (Head) после подмены меша, чтобы
	// часть продолжала анимироваться синхронно с телом (GDD §7.4). Для самого Head — no-op
	// (он и есть лидер). Использует тот же механизм, что и риг модульных частей базы.
	void RelinkSlotToLeaderPose(EArmorSlot Slot);

	// Запоминает базовые (надетые в BP) скелетные меши слотов — нужно для возврата при
	// снятии брони. Вызывается в BeginPlay (после применения дефолтов BP).
	void CacheBaseSlotMeshes();

private:
    float BaseWalkSpeed;
    float SprintMultiplier = 2.0f;
    bool IsSprinting;

    // Базовые меши слотов (тело без брони) — снимок BeginPlay для UnequipArmor.
    UPROPERTY()
    USkeletalMesh* BaseHeadMesh = nullptr;

    UPROPERTY()
    USkeletalMesh* BaseTorsoMesh = nullptr;

    UPROPERTY()
    USkeletalMesh* BaseLegsMesh = nullptr;

    bool bBaseSlotMeshesCached = false;
};
