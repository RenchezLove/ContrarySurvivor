// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "WolfCharacter.generated.h"

class UStatsComponent;
class UStaticMeshComponent;

/**
 * Волк — второй враг MVP (GDD §7.1, стайный для квеста).
 *
 * АРХИТЕКТУРА (решение Фазы 3): волк — квадрупед с ЕДИНЫМ мешем (tech-design:
 * «Звери (волки) — единый меш», НЕ модульный гуманоид). Поэтому НЕ наследует
 * AMasterHumanoidCharacter (модульная база Head/Torso/Legs), а идёт напрямую от
 * ACharacter. Общий enemy-функционал переиспользуется через КОМПОЗИЦИЮ:
 *   - UStatsComponent (HP/смерть) — как у бандита;
 *   - AEnemyAIController-логика через подкласс AWolfAIController (chase/attack).
 * AI-контроллер берёт UStatsComponent обобщённо (FindComponentByClass), без привязки
 * к классу пешки.
 *
 * МЕШ — ПЛЕЙСХОЛДЕР: простой статик-меш движка (цилиндр), чтобы волк был функционален
 * и виден без редактора. Реальный скелет-меш/анимации — позже (modeler-3d/unreal-operator).
 *
 * ЧЕРНОВЫЕ статы (draft): HP 40, скорость ~1.3× бандита. Урон укуса/дальность — на
 * AWolfAIController.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AWolfCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AWolfCharacter();

protected:
	virtual void BeginPlay() override;

	// Источник истины по HP волка (как у бандита — через компонент, минуя инлайн-логику).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true"))
	UStatsComponent* Stats;

	// Плейсхолдер-визуал (простой меш движка), пока нет реального скелет-меша волка.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components", meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* PlaceholderMesh;

	// Стартовое HP волка. ЧЕРНОВОЕ значение (draft, GDD §7.1).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	float WolfMaxHealth = 40.0f;

	// Множитель скорости относительно базовой скорости бандита (~600). ЧЕРНОВОЕ (draft).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement")
	float SpeedMultiplierVsBandit = 1.3f;

	// Через сколько секунд после смерти убрать тело.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death")
	float CorpseLifeSpan = 5.0f;

	UFUNCTION()
	void HandleDeath();

public:
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Stats")
	UStatsComponent* GetStats() const { return Stats; }
};
