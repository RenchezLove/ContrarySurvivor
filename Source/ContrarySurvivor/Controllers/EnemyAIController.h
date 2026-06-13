// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class UStatsComponent;

// Состояния примитивного state-machine (MVP без Behavior Tree, ADR/tech-design Фаза 1).
UENUM(BlueprintType)
enum class EEnemyAIState : uint8
{
	Idle    UMETA(DisplayName = "Idle"),     // нет цели / цель не видна
	Chase   UMETA(DisplayName = "Chase"),    // движется к игроку
	Attack  UMETA(DisplayName = "Attack")    // в радиусе, атакует с кулдауном
};

/**
 * Простой AI-контроллер врага: Idle -> Chase -> Attack.
 * Обнаружение: дистанция + линия видимости (LineOfSightTo).
 * Движение: MoveToActor. Атака: урон игроку через TakeDamage по кулдауну.
 */
UCLASS()
class CONTRARYSURVIVOR_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

	// --- Параметры восприятия/боя (тюнингуемые) ---

	// Дистанция обнаружения игрока (см). За пределами — Idle.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Perception")
	float DetectionRange = 1500.0f;

	// Дальность атаки ножом, измеряется ПОВЕРХНОСТЬ-К-ПОВЕРХНОСТИ капсул (см),
	// т.е. зазор между капсулами врага и игрока, а НЕ расстояние между их центрами.
	// Эффективная проверка центр-к-центру = AttackRange + (радиус капсулы врага + игрока).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat")
	float AttackRange = 90.0f;

	// Радиус приёмки для MoveToActor (см). Останавливаемся, не упираясь в игрока.
	// Должен быть таким, чтобы дистанция остановки преследования была <= дальности атаки
	// (с учётом радиусов капсул обоих). 60 < AttackRange(90) поверхность-к-поверхности.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Movement")
	float MoveAcceptanceRadius = 60.0f;

	// Урон одной атаки.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat")
	float AttackDamage = 10.0f;

	// Кулдаун между атаками (сек).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Combat")
	float AttackCooldown = 1.5f;

	// Текущее состояние (для отладки/привязок).
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|State")
	EEnemyAIState CurrentState = EEnemyAIState::Idle;

private:
	// Время последней атаки (по GetWorld()->GetTimeSeconds()).
	float LastAttackTime = -1000.0f;

	// Кэш статов своего пешки (для проверки "враг жив").
	UPROPERTY()
	UStatsComponent* OwnStats = nullptr;

	// Находит игрока-пешку (через PlayerController 0).
	APawn* GetPlayerPawn() const;

	// Видит ли врага игрока: дистанция в пределах DetectionRange + LineOfSightTo.
	bool CanSensePlayer(APawn* Player) const;

	// Сумма радиусов капсул врага и игрока (см). Нужна, чтобы дистанции боя/остановки
	// мерить поверхность-к-поверхности, а не центр-к-центру (GetDistanceTo даёт центры).
	float GetCombinedCapsuleRadius(APawn* Player) const;

	// Выполняет атаку по игроку (урон через TakeDamage).
	void PerformAttack(APawn* Player);
};
