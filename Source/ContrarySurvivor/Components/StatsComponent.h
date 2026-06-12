// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "StatsComponent.generated.h"

// --- Делегаты для привязки HUD/реакций (ADR-015: делегаты на изменение статов) ---

// Срабатывает при любом изменении здоровья. NewHealth — текущее, MaxHealth — максимум.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, MaxHealth);

// Срабатывает один раз при переходе здоровья в 0.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

/**
 * Тип операции модификатора стата (ADR-015: структура аддитив/мультипликатив).
 * ЗАДЕЛ: в Фазе 1 не применяется к логике, только хранится. Полноценная система — Фаза 2+.
 */
UENUM(BlueprintType)
enum class EStatModifierOp : uint8
{
	Additive       UMETA(DisplayName = "Additive"),       // прибавка/убавка (например, +20 к MaxHealth)
	Multiplicative UMETA(DisplayName = "Multiplicative")   // множитель (например, x1.2 к MoveSpeed)
};

/**
 * Модификатор одного стата (ЗАДЕЛ под броню/баффы по ADR-015).
 * В Фазе 1 хранится как структура-задел; применение/пересчёт — Фаза 2+.
 */
USTRUCT(BlueprintType)
struct FStatModifier
{
	GENERATED_BODY()

	// Источник модификатора (для удаления при снятии брони и т.п.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Modifier")
	FName SourceTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Modifier")
	EStatModifierOp Op = EStatModifierOp::Additive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats|Modifier")
	float Value = 0.0f;
};

/**
 * Лёгкий кастомный компонент статов (ADR-015: GAS НЕ используем).
 * Фаза 1: только Health + смерть. Остальные статы — задел-поля без логики деградации.
 */
UCLASS(ClassGroup = (Stats), meta = (BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatsComponent();

protected:
	virtual void BeginPlay() override;

	// --- Health (активно в Фазе 1) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Health")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Health")
	float Health = 100.0f;

	// Флаг чтобы OnDeath не вызвался дважды
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Health")
	bool bIsDead = false;

	// --- Задел-статы (Фаза 2+): объявлены, логики деградации НЕТ ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Survival")
	float Hunger = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Survival")
	float Thirst = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Survival")
	float Money = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Movement")
	float MoveSpeed = 600.0f;

	// Задел: список активных модификаторов статов (ADR-015). В Фазе 1 не применяется.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Modifier")
	TArray<FStatModifier> ActiveModifiers;

public:
	// --- Делегаты ---

	UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
	FOnDeath OnDeath;

	// --- API ---

	// Наносит урон (>0). Возвращает фактически снятое здоровье. Триггерит OnHealthChanged и (при 0) OnDeath.
	UFUNCTION(BlueprintCallable, Category = "Stats|Health")
	float ApplyDamage(float DamageAmount);

	// Лечит (>0). Не превышает MaxHealth. Не лечит мёртвых.
	UFUNCTION(BlueprintCallable, Category = "Stats|Health")
	float Heal(float HealAmount);

	UFUNCTION(BlueprintPure, Category = "Stats|Health")
	FORCEINLINE bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "Stats|Health")
	FORCEINLINE float GetHealth() const { return Health; }

	UFUNCTION(BlueprintPure, Category = "Stats|Health")
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Stats|Health")
	float GetHealthPercent() const;

	// Задаёт MaxHealth и при желании выставляет Health в максимум (для инициализации врага).
	UFUNCTION(BlueprintCallable, Category = "Stats|Health")
	void InitHealth(float InMaxHealth, bool bSetToMax = true);

	// --- ЗАДЕЛ-API под модификаторы (ADR-015), без сложной системы ---

	// Добавить модификатор в список. В Фазе 1 НЕ пересчитывает статы (только хранит). Расширяемость.
	UFUNCTION(BlueprintCallable, Category = "Stats|Modifier")
	void AddModifier(const FStatModifier& Modifier);

	// Удалить все модификаторы по тегу-источнику (например, при снятии брони).
	UFUNCTION(BlueprintCallable, Category = "Stats|Modifier")
	void RemoveModifiersBySource(FName SourceTag);
};
