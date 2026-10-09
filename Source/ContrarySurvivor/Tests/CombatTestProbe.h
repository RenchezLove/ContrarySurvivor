// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CombatTestProbe.generated.h"

/**
 * Лёгкий UObject-зонд для headless Automation-тестов боевого кора.
 *
 * Динамические мультикаст-делегаты (UStatsComponent::OnDeath и т.п.) можно привязать ТОЛЬКО
 * к UFUNCTION на UObject (AddDynamic). Этот зонд даёт такие UFUNCTION-обработчики со
 * счётчиками, чтобы тест мог проверить, что делегат реально сработал нужное число раз.
 * Используется исключительно тестами (Source/ContrarySurvivor/Tests).
 */
UCLASS()
class CONTRARYSURVIVOR_API UCombatTestProbe : public UObject
{
	GENERATED_BODY()

public:
	// Счётчик срабатываний OnDeath.
	int32 DeathCount = 0;

	// Последнее значение из OnMoneyChanged (для проверки начисления награды).
	float LastMoney = -1.0f;
	int32 MoneyChangedCount = 0;

	UFUNCTION()
	void HandleDeath() { ++DeathCount; }

	UFUNCTION()
	void HandleMoneyChanged(float NewMoney) { LastMoney = NewMoney; ++MoneyChangedCount; }

	// Урон по мишени (AActor::OnTakeAnyDamage): сколько раз мишень получила урон и сколько
	// всего — тесты дроби проверяют «один вызов урона на залп» и сумму по дробинам.
	int32 AnyDamageCount = 0;
	float TotalAnyDamage = 0.0f;
	float LastAnyDamage = 0.0f;

	UFUNCTION()
	void HandleAnyDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType,
		class AController* InstigatedBy, AActor* DamageCauser)
	{
		++AnyDamageCount;
		TotalAnyDamage += Damage;
		LastAnyDamage = Damage;
	}
};
