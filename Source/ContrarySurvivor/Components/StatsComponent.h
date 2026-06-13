// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "StatsComponent.generated.h"

// --- Делегаты для привязки HUD/реакций (ADR-015: делегаты на изменение статов) ---

// Срабатывает при любом изменении здоровья. NewHealth — текущее, MaxHealth — максимум.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, MaxHealth);

// Срабатывает один раз при переходе здоровья в 0.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDeath);

// Изменение голода. NewHunger — текущее, MaxValue — максимум (для HUD).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHungerChanged, float, NewHunger, float, MaxValue);

// Изменение жажды. NewThirst — текущее, MaxValue — максимум (для HUD).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThirstChanged, float, NewThirst, float, MaxValue);

// Изменение денег. NewMoney — текущее значение (для HUD).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoneyChanged, float, NewMoney);

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
 * Лёгкий кастомный компонент статов (ADR-015: GAS НЕ используем, на float).
 * Фаза 1: Health + смерть. Фаза 2 (GDD §7.3): голод/жажда/деньги, деградация по таймеру,
 * критический порог -> падение HP, методы восстановления (еда/вода).
 * Расширяемость под модификаторы (FStatModifier) сохранена.
 */
UCLASS(ClassGroup = (Stats), meta = (BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStatsComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- Health (активно в Фазе 1) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Health")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Health")
	float Health = 100.0f;

	// Флаг чтобы OnDeath не вызвался дважды
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Health")
	bool bIsDead = false;

	// --- Выживание (Фаза 2, GDD §7.3) ---

	// Максимум голода/жажды (GDD §7.3: макс 100).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival")
	float SurvivalMax = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Survival")
	float Hunger = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Survival")
	float Thirst = 100.0f;

	// Деньги. Стартовое значение по GDD §7.6 = 50.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats|Survival")
	float Money = 50.0f;

	// Включать ли деградацию голода/жажды по таймеру (для врага — выкл, для игрока — вкл).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival")
	bool bEnableSurvivalDegradation = false;

	// --- Тюнинг деградации (GDD §7.3, числа дословно) ---

	// Жажда -1 / 8 c.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float ThirstDrainInterval = 8.0f;

	// Голод -1 / 12 c.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float HungerDrainInterval = 12.0f;

	// Шаг убыли голода/жажды за тик таймера.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float SurvivalDrainStep = 1.0f;

	// Критический порог: при значении <= этого начинает падать HP (GDD §7.3: <= 20).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float CriticalThreshold = 20.0f;

	// От голода: -1 HP / 3 c (период тика урона от голода).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float HungerHealthDrainInterval = 3.0f;

	// От жажды: -1 HP / 2 c (период тика урона от жажды).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float ThirstHealthDrainInterval = 2.0f;

	// Сколько HP снимается за один критический тик голода/жажды (суммируются, если оба критичны).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float CriticalHealthDrainStep = 1.0f;

	// Восстановление едой (+Hunger) — GDD §7.3.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float FoodRestoreAmount = 30.0f;

	// Восстановление водой (+Thirst) — GDD §7.3.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float WaterRestoreAmount = 40.0f;

	// DRAFT (запрос Рината, Фаза 4): еда дополнительно чуть лечит HP (clamp до MaxHealth).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float FoodHealthRestoreAmount = 5.0f;

	// DRAFT (запрос Рината, Фаза 4): вода дополнительно чуть лечит HP (clamp до MaxHealth).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Tuning")
	float WaterHealthRestoreAmount = 3.0f;

	// --- Авто-реген HP при сытости (DRAFT, запрос Рината, Фаза 4) ---

	// Включать ли авто-реген HP (только для игрока; для врага — выкл, как и деградация).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Regen")
	bool bEnableHealthRegen = true;

	// DRAFT: реген идёт, только если Hunger >= этого порога.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Regen")
	float RegenHungerThreshold = 80.0f;

	// DRAFT: реген идёт, только если Thirst >= этого порога.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Regen")
	float RegenThirstThreshold = 80.0f;

	// DRAFT: сколько HP восстанавливается за один тик регена.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Regen")
	float HealthRegenAmount = 2.0f;

	// DRAFT: период тика авто-регена HP, c (укладывается в «1-3 пункта за 30-50 c»).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats|Survival|Regen")
	float HealthRegenInterval = 40.0f;

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

	UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
	FOnHungerChanged OnHungerChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
	FOnThirstChanged OnThirstChanged;

	UPROPERTY(BlueprintAssignable, Category = "Stats|Events")
	FOnMoneyChanged OnMoneyChanged;

	// --- API ---

	// Наносит урон (>0). Возвращает фактически снятое здоровье. Триггерит OnHealthChanged и (при 0) OnDeath.
	UFUNCTION(BlueprintCallable, Category = "Stats|Health")
	float ApplyDamage(float DamageAmount);

	// Лечит (>0). Не превышает MaxHealth. Не лечит мёртвых.
	UFUNCTION(BlueprintCallable, Category = "Stats|Health")
	float Heal(float HealAmount);

	// Жёстко выставляет здоровье (clamp 0..MaxHealth), пересчитывает bIsDead и бродкастит.
	// В отличие от Heal — работает и на «мёртвом» компоненте (нужно для респауна).
	UFUNCTION(BlueprintCallable, Category = "Stats|Health")
	void SetHealth(float NewHealth);

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

	// Включает/выключает деградацию голода/жажды. Если вызвано в игре (есть World) —
	// сразу запускает/останавливает таймеры. Для игрока ставится в конструкторе.
	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	void SetSurvivalDegradationEnabled(bool bEnabled);

	// --- Выживание: геттеры (для HUD/сейва) ---

	UFUNCTION(BlueprintPure, Category = "Stats|Survival")
	FORCEINLINE float GetHunger() const { return Hunger; }

	UFUNCTION(BlueprintPure, Category = "Stats|Survival")
	FORCEINLINE float GetThirst() const { return Thirst; }

	UFUNCTION(BlueprintPure, Category = "Stats|Survival")
	FORCEINLINE float GetMoney() const { return Money; }

	UFUNCTION(BlueprintPure, Category = "Stats|Survival")
	FORCEINLINE float GetSurvivalMax() const { return SurvivalMax; }

	// Голод/жажда в критической зоне (<= порога) — для HUD-индикаторов и логики.
	UFUNCTION(BlueprintPure, Category = "Stats|Survival")
	bool IsHungerCritical() const { return Hunger <= CriticalThreshold; }

	UFUNCTION(BlueprintPure, Category = "Stats|Survival")
	bool IsThirstCritical() const { return Thirst <= CriticalThreshold; }

	// --- Выживание: восстановление (задел под предметы, GDD §7.3) ---

	// Еда: +FoodRestoreAmount к голоду (clamp 0..SurvivalMax).
	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	void ConsumeFood();

	// Вода: +WaterRestoreAmount к жажде (clamp 0..SurvivalMax).
	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	void DrinkWater();

	// Прямое добавление к голоду/жажде (универсально под предметы). Может быть отрицательным.
	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	void ModifyHunger(float Delta);

	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	void ModifyThirst(float Delta);

	// Жёстко выставляет голод/жажду (clamp 0..SurvivalMax) и бродкастит. По образцу SetHealth —
	// нужно для death-респауна (форс полных значений независимо от сейва).
	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	void SetHunger(float NewHunger);

	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	void SetThirst(float NewThirst);

	// --- Деньги ---

	// Инициализирует деньги для НОВОГО персонажа (стартовое значение, GDD §7.6 = 50).
	// НЕ для загрузки сейва (для сейва — RestoreState). clamp >=0 + бродкаст HUD.
	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	void InitMoney(float StartingAmount);

	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	void AddMoney(float Amount);

	// Списывает деньги, если хватает. Возвращает true при успехе.
	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	bool SpendMoney(float Amount);

	// --- Сейв/респаун: восстановление состояния (Фаза 2, GDD §7.8) ---

	// Полностью восстанавливает состояние статов из сейва: снимает флаг смерти,
	// выставляет значения и бродкастит делегаты. Перезапускает таймеры деградации.
	UFUNCTION(BlueprintCallable, Category = "Stats|Survival")
	void RestoreState(float InHealth, float InHunger, float InThirst, float InMoney);

	// --- ЗАДЕЛ-API под модификаторы (ADR-015), без сложной системы ---

	// Добавить модификатор в список. В Фазе 1 НЕ пересчитывает статы (только хранит). Расширяемость.
	UFUNCTION(BlueprintCallable, Category = "Stats|Modifier")
	void AddModifier(const FStatModifier& Modifier);

	// Удалить все модификаторы по тегу-источнику (например, при снятии брони).
	UFUNCTION(BlueprintCallable, Category = "Stats|Modifier")
	void RemoveModifiersBySource(FName SourceTag);

private:
	// --- Таймеры деградации (запускаются в BeginPlay, если bEnableSurvivalDegradation) ---
	FTimerHandle ThirstDrainTimer;
	FTimerHandle HungerDrainTimer;
	FTimerHandle HungerHealthTimer;
	FTimerHandle ThirstHealthTimer;
	// Авто-реген HP при сытости (DRAFT, Фаза 4). Логически независим от деградации.
	FTimerHandle HealthRegenTimer;

	// Запуск/останов таймеров деградации.
	void StartSurvivalTimers();
	void StopSurvivalTimers();

	// Колбэки таймеров.
	void TickThirstDrain();
	void TickHungerDrain();
	void TickHungerHealthDrain();
	void TickThirstHealthDrain();
	// Колбэк авто-регена HP: лечит, только если сыт И не испытывает жажды И HP < Max И жив.
	void TickHealthRegen();
};
