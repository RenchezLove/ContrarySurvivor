// Fill out your copyright notice in the Description page of Project Settings.

#include "StatsComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h" // FadeOut куска стона (фикс «бесконечных охов»)
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Debug/QADebug.h"     // QA god-mode (заморозка деградации) + MONEY-лог
#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h" // громкость эффектов (экран настроек)
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h" // #2: флаг спринта владельца
#include "ContrarySurvivor/Characters/PlayerCharacter.h" // ADR-063: приглушение истощения (квест/наигрыш)

UStatsComponent::UStatsComponent()
{
	// Компонент статов не тикает (логика деградации статов — Фаза 2+).
	PrimaryComponentTick.bCanEverTick = false;

	// Звук получения урона (Демо). Дефолт из импортированного ассета; переопределяется в BP.
	static ConstructorHelpers::FObjectFinder<USoundBase> HurtSoundAsset(
		TEXT("/Game/Audio/Demo/death_pain_grunts.death_pain_grunts"));
	if (HurtSoundAsset.Succeeded())
	{
		HurtSound = HurtSoundAsset.Object;
	}
}

void UStatsComponent::PlayHurtSound()
{
	UWorld* World = GetWorld();
	if (!HurtSound || !World)
	{
		return;
	}

	// Анти-спам (фикс «бесконечных охов», Ринат 07-17): предыдущий PlaySoundAtLocation играл
	// длинный многостонный файл целиком на КАЖДОЕ попадание — копии накладывались и «охали»
	// долго после боя. Теперь: не чаще HurtSoundMinInterval на персонаже...
	const float Now = World->GetTimeSeconds();
	if (LastHurtSoundTime >= 0.0f && (Now - LastHurtSoundTime) < HurtSoundMinInterval)
	{
		return;
	}
	LastHurtSoundTime = Now;

	const AActor* Owner = GetOwner();
	const FVector Loc = Owner ? Owner->GetActorLocation() : FVector::ZeroVector;

	// ...и играется СЛУЧАЙНЫЙ короткий кусок файла (каждый раз другой «ох»), а не файл целиком.
	// Duration >= INDEFINITELY_LOOPING_DURATION означает «зациклен/неизвестно» — тогда с начала.
	const float Duration = HurtSound->GetDuration();
	float StartTime = 0.0f;
	if (Duration > HurtSoundSliceDuration && Duration < INDEFINITELY_LOOPING_DURATION)
	{
		StartTime = FMath::FRandRange(0.0f, Duration - HurtSoundSliceDuration);
	}

	UAudioComponent* Audio = UGameplayStatics::SpawnSoundAtLocation(
		this, HurtSound, Loc, FRotator::ZeroRotator,
		// Громкость эффектов с экрана настроек (ADR-062).
		HurtSoundVolume * UContrarySurvivorGameUserSettings::GetEffectsVolumeSafe(),
		/*PitchMultiplier=*/1.0f, StartTime);
	if (!Audio)
	{
		return;
	}

	// Обрезаем кусок по таймеру: FadeOut плавно гасит и останавливает компонент,
	// bAutoDestroy (дефолт SpawnSoundAtLocation) затем прибирает его сам.
	TWeakObjectPtr<UAudioComponent> WeakAudio = Audio;
	FTimerHandle SliceTimer;
	World->GetTimerManager().SetTimer(SliceTimer, [WeakAudio]()
	{
		if (UAudioComponent* Active = WeakAudio.Get())
		{
			Active->FadeOut(/*FadeOutDuration=*/0.15f, /*FadeVolumeLevel=*/0.0f);
		}
	}, HurtSoundSliceDuration, /*bLoop=*/false);
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

void UStatsComponent::SetHunger(float NewHunger)
{
	Hunger = FMath::Clamp(NewHunger, 0.0f, SurvivalMax);
	OnHungerChanged.Broadcast(Hunger, SurvivalMax);
}

void UStatsComponent::SetThirst(float NewThirst)
{
	Thirst = FMath::Clamp(NewThirst, 0.0f, SurvivalMax);
	OnThirstChanged.Broadcast(Thirst, SurvivalMax);
}

void UStatsComponent::ConsumeFood()
{
	const float OldHealth = Health;
	ModifyHunger(FoodRestoreAmount);
	// DRAFT (Фаза 4): еда дополнительно чуть лечит HP. Heal сам clamp'ит до MaxHealth
	// и не лечит мёртвых (вернёт 0) — деградация/урон при этом не затрагиваются.
	if (FoodHealthRestoreAmount > 0.0f)
	{
		Heal(FoodHealthRestoreAmount);
	}
	UE_LOG(LogQA, Display, TEXT("QA: ate food (+%.0f hunger, +%.1f HP). Hunger %.0f/%.0f, HP %.1f->%.1f/%.1f"),
		FoodRestoreAmount, Health - OldHealth, Hunger, SurvivalMax, OldHealth, Health, MaxHealth);
}

void UStatsComponent::DrinkWater()
{
	const float OldHealth = Health;
	ModifyThirst(WaterRestoreAmount);
	// DRAFT (Фаза 4): вода дополнительно чуть лечит HP (clamp до MaxHealth, не лечит мёртвых).
	if (WaterHealthRestoreAmount > 0.0f)
	{
		Heal(WaterHealthRestoreAmount);
	}
	UE_LOG(LogQA, Display, TEXT("QA: drank water (+%.0f thirst, +%.1f HP). Thirst %.0f/%.0f, HP %.1f->%.1f/%.1f"),
		WaterRestoreAmount, Health - OldHealth, Thirst, SurvivalMax, OldHealth, Health, MaxHealth);
}

void UStatsComponent::InitMoney(float StartingAmount)
{
	// Стартовые деньги НОВОГО персонажа (не загрузка сейва). Перетирать загруженный сейв
	// этим нельзя: вызывается только из BeginPlay игрока (новый игрок), а RestoreState
	// (загрузка) идёт отдельным путём (смерть/костёр).
	Money = FMath::Max(0.0f, StartingAmount);
	OnMoneyChanged.Broadcast(Money);
	UE_LOG(LogQA, Display, TEXT("QA: starting money initialised -> %.0f"), Money);
}

void UStatsComponent::AddMoney(float Amount)
{
	if (Amount == 0.0f)
	{
		return;
	}
	Money = FMath::Max(0.0f, Money + Amount);
	OnMoneyChanged.Broadcast(Money);
	// QA-инструментирование: начисление/списание денег + итоговый баланс (виден в оверлее).
	FQADebug::QA(this, FString::Printf(TEXT("QA: MONEY %+.0f -> balance %.0f"), Amount, Money), /*bScreen=*/true);
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

	// DRAFT (Фаза 4): авто-реген HP при сытости. Таймер тикает наравне с деградацией;
	// фактический реген применяется только при выполнении условий (см. TickHealthRegen).
	// Привязан к тем же владельцам, что и деградация (игрок) — у врага survival выкл, реген не стартует.
	if (bEnableHealthRegen && HealthRegenInterval > 0.0f)
	{
		TM.SetTimer(HealthRegenTimer, this, &UStatsComponent::TickHealthRegen, HealthRegenInterval, true);
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
	TM.ClearTimer(HealthRegenTimer);
}

void UStatsComponent::TickThirstDrain()
{
	// QA god-mode (клавиша T): убыль голода/жажды заморожена (только у игрока есть деградация).
	if (bIsDead || FQADebug::bGodMode)
	{
		return;
	}
	// #2: при спринте расход жажды множится на SprintDrainMultiplier.
	float Step = SurvivalDrainStep;
	const AMasterHumanoidCharacter* OwnerChar = Cast<AMasterHumanoidCharacter>(GetOwner());
	if (OwnerChar && OwnerChar->GetIsSprinting())
	{
		Step *= SprintDrainMultiplier;
		UE_LOG(LogQA, Display, TEXT("QA: sprint thirst drain -%.1f (x%.1f) -> %.0f/%.0f"),
			Step, SprintDrainMultiplier, FMath::Max(Thirst - Step, 0.0f), SurvivalMax);
	}
	ModifyThirst(-Step);
}

void UStatsComponent::TickHungerDrain()
{
	if (bIsDead || FQADebug::bGodMode)
	{
		return;
	}
	// #2: при спринте расход голода множится на SprintDrainMultiplier.
	float Step = SurvivalDrainStep;
	const AMasterHumanoidCharacter* OwnerChar = Cast<AMasterHumanoidCharacter>(GetOwner());
	if (OwnerChar && OwnerChar->GetIsSprinting())
	{
		Step *= SprintDrainMultiplier;
		UE_LOG(LogQA, Display, TEXT("QA: sprint hunger drain -%.1f (x%.1f) -> %.0f/%.0f"),
			Step, SprintDrainMultiplier, FMath::Max(Hunger - Step, 0.0f), SurvivalMax);
	}
	ModifyHunger(-Step);
}

void UStatsComponent::TickHungerHealthDrain()
{
	// При критическом голоде HP падает. Суммирование с жаждой — за счёт двух
	// независимых таймеров, каждый снимает HP отдельно (GDD §7.3).
	// God-mode (T) замораживает и критический урон от голода.
	// На обучении урон истощения приглушён (ADR-063: шкалы убывают, HP цел).
	if (!bIsDead && !FQADebug::bGodMode && Hunger <= CriticalThreshold
		&& !IsStarvationDamageSuppressed())
	{
		ApplyDamage(CriticalHealthDrainStep);
	}
}

void UStatsComponent::TickThirstHealthDrain()
{
	// На обучении урон истощения приглушён (ADR-063: шкалы убывают, HP цел).
	if (!bIsDead && !FQADebug::bGodMode && Thirst <= CriticalThreshold
		&& !IsStarvationDamageSuppressed())
	{
		ApplyDamage(CriticalHealthDrainStep);
	}
}

bool UStatsComponent::IsStarvationDamageSuppressed() const
{
	if (!bStarvationGraceEnabled)
	{
		return false;
	}
	// Механика обучения ИГРОКА: на прочих владельцах приглушения нет (у врагов деградация
	// и так выключена — это страховка на случай включения survival-статов кому-то ещё).
	const APlayerCharacter* Player = Cast<APlayerCharacter>(GetOwner());
	if (!Player)
	{
		return false;
	}
	if (Player->HasTurnedInFirstQuest())
	{
		return false; // первый квест сдан — обучение позади, истощение бьёт как обычно
	}
	if (Player->GetTotalPlayTimeSeconds() >= StarvationGraceMinutes * 60.0f)
	{
		return false; // страховка от вечного бессмертия: наигран порог — приглушение снято
	}
	if (!bStarvationGraceLogged)
	{
		bStarvationGraceLogged = true;
		UE_LOG(LogQA, Display,
			TEXT("QA: урон истощения приглушён (обучение, ADR-063) — до первого сданного квеста либо %.0f мин игры"),
			StarvationGraceMinutes);
	}
	return true;
}

void UStatsComponent::TickHealthRegen()
{
	// DRAFT (Фаза 4): медленный авто-реген HP при сытости.
	// Условия (логически независимы от деградации, которая всегда тикает отдельно):
	//   жив, HP < MaxHealth, сыт (Hunger >= порога) И не испытывает жажды (Thirst >= порога).
	// При падении сытости ниже порога реген просто перестаёт применяться (таймер продолжает тикать).
	if (bIsDead || Health >= MaxHealth)
	{
		return;
	}
	if (Hunger >= RegenHungerThreshold && Thirst >= RegenThirstThreshold)
	{
		const float OldHealth = Health;
		const float Healed = Heal(HealthRegenAmount);
		if (Healed > 0.0f)
		{
			UE_LOG(LogQA, Display, TEXT("QA: auto-regen tick +%.1f HP (%.1f->%.1f/%.1f), Hunger %.0f Thirst %.0f"),
				Healed, OldHealth, Health, MaxHealth, Hunger, Thirst);
		}
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
