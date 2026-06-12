// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MasterHumanoidCharacter.h"
#include "EnemyCharacter.generated.h"

class UStatsComponent;

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

	// Компонент статов (Health/смерть). Источник истины по HP для врага.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
	UStatsComponent* Stats;

	// Стартовое здоровье бандита. Черновое тюнингуемое значение.
	// 80 HP = 4 попадания из пистолета (25 урона/выстрел, 2 выстр/с) ≈ 2 сек огня. Тюнится.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	float BanditMaxHealth = 80.0f;

	// Через сколько секунд после смерти Destroy тела (даём отыграть рэгдолл).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
	float CorpseLifeSpan = 5.0f;

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
