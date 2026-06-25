// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MasterHumanoidCharacter.h"
#include "EnemyCharacter.generated.h"

class UStatsComponent;
class AMasterInventoryItem;
class APickup;

/**
 * Враг первого вертикального среза (бандит).
 * Наследует AMasterHumanoidCharacter КАК ЕСТЬ (ADR-018: модульность остаётся в базе).
 * Несёт UStatsComponent (ADR-015) — здоровье и смерть идут через него, минуя инлайн-Health базы.
 * AI управляет AEnemyAIController (назначается через AutoPossessAI/BP).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AEnemyCharacter : public AMasterHumanoidCharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

protected:
	virtual void BeginPlay() override;

	// ПРАВКА B: связываем модульные части (Torso/Legs) с Head через Leader Pose,
	// чтобы они следовали позе/анимации Head как единое тело.
	virtual void PostInitializeComponents() override;

	// Компонент статов (Health/смерть). Источник истины по HP для врага.
	// meta DisplayPriority — поднять наши настройки наверх Details (фидбек Рината), сразу после Transform.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	UStatsComponent* Stats;

	// Стартовое здоровье бандита. Черновое тюнингуемое значение.
	// 80 HP = 4 попадания из пистолета (25 урона/выстрел, 2 выстр/с) ≈ 2 сек огня. Тюнится.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	float BanditMaxHealth = 80.0f;

	// Скорость погони бандита (см/с). TUNING. Чуть ВЫШЕ скорости ходьбы игрока (~600), чтобы
	// бандит реально догонял шагающего игрока, но НИЖЕ спринта игрока (~1200) — от спринта можно
	// оторваться ценой расхода голода/жажды (бой остаётся проходимым). Применяется детерминированно
	// в BeginPlay (после Super), чтобы не зависеть от дефолта CMC/возможного оверрайда в BP.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (DisplayPriority = "2"))
	float BanditWalkSpeed = 650.0f;

	// Через сколько секунд после смерти Destroy тела (даём отыграть рэгдолл).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death", meta = (DisplayPriority = "3"))
	float CorpseLifeSpan = 5.0f;

	// --- Лут при смерти (GDD §7.8: «враги дают деньги, изношенное оружие») ---
	// Деньги: случайно в [LootMoneyMin..Max]. Бандит DRAFT 10-30 (GDD §7.6 экономика).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (DisplayPriority = "4"))
	float LootMoneyMin = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	float LootMoneyMax = 30.0f;

	// Шанс выпадения предмета (расходник/изношенный лут). DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float LootItemDropChance = 0.35f;

	// Класс выпадающего предмета (по умолчанию расходник). Спавнится скрытым в пикапе.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TSubclassOf<AMasterInventoryItem> LootItemClass;

	// Класс пикапа-лута (по умолчанию APickup, без BP/редактора).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TSubclassOf<APickup> PickupClass;

	// Спавнит лут (деньги + шанс предмета) в позиции трупа. Вызывается из HandleDeath.
	void DropLoot();

	// Реакция на смерть из делегата UStatsComponent::OnDeath.
	// Переопределяет базовую заглушку: у врага смерть идёт через UStatsComponent,
	// поэтому здесь — полноценная обработка (рэгдолл, отключение ИИ, Destroy с задержкой).
	// UFUNCTION-спецификатор НЕ повторяем (наследуется от базовой virtual UFUNCTION) —
	// этого достаточно для AddDynamic к делегату OnDeath.
	virtual void HandleDeath() override;

public:
	// Перехватываем стандартный пайплайн урона UE и роутим в UStatsComponent,
	// чтобы существующая система оружия (Weapon->TakeDamage) работала без правок.
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Stats")
	UStatsComponent* GetStats() const { return Stats; }
};
