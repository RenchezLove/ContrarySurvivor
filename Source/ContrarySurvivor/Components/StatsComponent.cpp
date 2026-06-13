// Fill out your copyright notice in the Description page of Project Settings.

#include "StatsComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
	Hunger = FMath::Clamp(Hunger, 0.0f, SurvivalMax);
	Thirst = FMath::Clamp(Thirst, 0.0f, SurvivalMax);
	bIsDead = (Health <= 0.0f);

	// Стартовый бродкаст, чтобы HUD/хелсбар сразу получили актуальные значения.
	OnHealthChanged.Broadcast(Health, MaxHealth);
	OnHungerChanged.Broadcast(Hunger, SurvivalMax);
	OnThirstChanged.Broadcast(Thirst, SurvivalMax);
	OnMoneyChanged.Broadcast(Money);

	if (bEnableSurvivalDegradation)
	{
		StartSurvivalTimers();
	}
}

void UStatsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSurvivalTimers();
	Super::EndPlay(EndPlayReason);
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

void UStatsComponent::SetHealth(float NewHealth)
{
	Health = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	bIsDead = (Health <= 0.0f);
	OnHealthChanged.Broadcast(Health, MaxHealth);
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

// ---------------------------------------------------------------------------
// Выживание: голод / жажда / деньги (GDD §7.3, §7.6)
// ---------------------------------------------------------------------------

void UStatsComponent::SetSurvivalDegradationEnabled(bool bEnabled)
{
	bEnableSurvivalDegradation = bEnabled;

	// Если уже в игре — синхронизируем таймеры немедленно.
	if (HasBegunPlay())
	{
		if (bEnabled)
		{
			StartSurvivalTimers();
		}
		else
		{
			StopSurvivalTimers();
		}
	}
}

void UStatsComponent::ModifyHunger(float Delta)
{
	if (Delta == 0.0f)
	{
		return;
	}
	const float Old = Hunger;
	Hunger = FMath::Clamp(Hunger + Delta, 0.0f, SurvivalMax);
	if (Hunger != Old)
	{
		OnHungerChanged.Broadcast(Hunger, SurvivalMax);
	}
}

void UStatsComponent::ModifyThirst(float Delta)
{
	if (Delta == 0.0f)
	{
		return;
	}
	const float Old = Thirst;
	Thirst = FMath::Clamp(Thirst + Delta, 0.0f, SurvivalMax);
	if (Thirst != Old)
	{
		OnThirstChanged.Broadcast(Thirst, SurvivalMax);
	}
}

void UStatsComponent::ConsumeFood()
{
	ModifyHunger(FoodRestoreAmount);
}

void UStatsComponent::DrinkWater()
{
	ModifyThirst(WaterRestoreAmount);
}

void UStatsComponent::AddMoney(float Amount)
{
	if (Amount == 0.0f)
	{
		return;
	}
	Money = FMath::Max(0.0f, Money + Amount);
	OnMoneyChanged.Broadcast(Money);
}

bool UStatsComponent::SpendMoney(float Amount)
{
	if (Amount <= 0.0f || Money < Amount)
	{
		return false;
	}
	Money -= Amount;
	OnMoneyChanged.Broadcast(Money);
	return true;
}

void UStatsComponent::RestoreState(float InHealth, float InHunger, float InThirst, float InMoney)
{
	// Снимаем смерть и восстанавливаем значения (респаун из сейва, GDD §7.8).
	bIsDead = false;
	Health = FMath::Clamp(InHealth, 0.0f, MaxHealth);
	Hunger = FMath::Clamp(InHunger, 0.0f, SurvivalMax);
	Thirst = FMath::Clamp(InThirst, 0.0f, SurvivalMax);
	Money = FMath::Max(0.0f, InMoney);

	// Если восстановили в 0 HP — считаем мёртвым (защита от некорректного сейва).
	bIsDead = (Health <= 0.0f);

	OnHealthChanged.Broadcast(Health, MaxHealth);
	OnHungerChanged.Broadcast(Hunger, SurvivalMax);
	OnThirstChanged.Broadcast(Thirst, SurvivalMax);
	OnMoneyChanged.Broadcast(Money);

	// Перезапуск таймеров деградации (они могли быть активны до смерти).
	if (bEnableSurvivalDegradation && !bIsDead)
	{
		StartSurvivalTimers();
	}
}

// ---------------------------------------------------------------------------
// Таймеры деградации
// ---------------------------------------------------------------------------

void UStatsComponent::StartSurvivalTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TM = World->GetTimerManager();

	if (ThirstDrainInterval > 0.0f)
	{
		TM.SetTimer(ThirstDrainTimer, this, &UStatsComponent::TickThirstDrain, ThirstDrainInterval, true);
	}
	if (HungerDrainInterval > 0.0f)
	{
		TM.SetTimer(HungerDrainTimer, this, &UStatsComponent::TickHungerDrain, HungerDrainInterval, true);
	}
	if (HungerHealthDrainInterval > 0.0f)
	{
		TM.SetTimer(HungerHealthTimer, this, &UStatsComponent::TickHungerHealthDrain, HungerHealthDrainInterval, true);
	}
	if (ThirstHealthDrainInterval > 0.0f)
	{
		TM.SetTimer(ThirstHealthTimer, this, &UStatsComponent::TickThirstHealthDrain, ThirstHealthDrainInterval, true);
	}
}

void UStatsComponent::StopSurvivalTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FTimerManager& TM = World->GetTimerManager();
	TM.ClearTimer(ThirstDrainTimer);
	TM.ClearTimer(HungerDrainTimer);
	TM.ClearTimer(HungerHealthTimer);
	TM.ClearTimer(ThirstHealthTimer);
}

void UStatsComponent::TickThirstDrain()
{
	if (bIsDead)
	{
		return;
	}
	ModifyThirst(-SurvivalDrainStep);
}

void UStatsComponent::TickHungerDrain()
{
	if (bIsDead)
	{
		return;
	}
	ModifyHunger(-SurvivalDrainStep);
}

void UStatsComponent::TickHungerHealthDrain()
{
	// При критическом голоде HP падает. Суммирование с жаждой — за счёт двух
	// независимых таймеров, каждый снимает HP отдельно (GDD §7.3).
	if (!bIsDead && Hunger <= CriticalThreshold)
	{
		ApplyDamage(CriticalHealthDrainStep);
	}
}

void UStatsComponent::TickThirstHealthDrain()
{
	if (!bIsDead && Thirst <= CriticalThreshold)
	{
		ApplyDamage(CriticalHealthDrainStep);
	}
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
