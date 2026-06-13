// Fill out your copyright notice in the Description page of Project Settings.

#include "StatsComponent.h"

UStatsComponent::UStatsComponent()
{
	// Компонент статов не тикает (логика деградации статов — Фаза 2+).
	PrimaryComponentTick.bCanEverTick = false;
}

void UStatsComponent::BeginPlay()
{
	Super::BeginPlay();

	// Гарантируем согласованность на старте.
	Health = FMath::Clamp(Health, 0.0f, MaxHealth);
	bIsDead = (Health <= 0.0f);

	// Стартовый бродкаст, чтобы HUD/хелсбар сразу получили актуальное значение.
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

float UStatsComponent::ApplyDamage(float DamageAmount)
{
	if (bIsDead || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float OldHealth = Health;
	Health = FMath::Max(Health - DamageAmount, 0.0f);
	const float ActualDamage = OldHealth - Health;

	OnHealthChanged.Broadcast(Health, MaxHealth);

	if (Health <= 0.0f && !bIsDead)
	{
		bIsDead = true;
		OnDeath.Broadcast();
	}

	return ActualDamage;
}

float UStatsComponent::Heal(float HealAmount)
{
	if (bIsDead || HealAmount <= 0.0f)
	{
		return 0.0f;
	}

	const float OldHealth = Health;
	Health = FMath::Min(Health + HealAmount, MaxHealth);
	const float ActualHeal = Health - OldHealth;

	if (ActualHeal > 0.0f)
	{
		OnHealthChanged.Broadcast(Health, MaxHealth);
	}

	return ActualHeal;
}

float UStatsComponent::GetHealthPercent() const
{
	return (MaxHealth > 0.0f) ? (Health / MaxHealth) : 0.0f;
}

void UStatsComponent::InitHealth(float InMaxHealth, bool bSetToMax)
{
	MaxHealth = FMath::Max(InMaxHealth, 1.0f);

	if (bSetToMax)
	{
		Health = MaxHealth;
		bIsDead = false;
	}
	else
	{
		Health = FMath::Clamp(Health, 0.0f, MaxHealth);
	}

	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void UStatsComponent::AddModifier(const FStatModifier& Modifier)
{
	// ЗАДЕЛ (ADR-015): в Фазе 1 только сохраняем. Пересчёт статов — Фаза 2+.
	ActiveModifiers.Add(Modifier);
}

void UStatsComponent::RemoveModifiersBySource(FName SourceTag)
{
	ActiveModifiers.RemoveAll([SourceTag](const FStatModifier& M)
	{
		return M.SourceTag == SourceTag;
	});
}
