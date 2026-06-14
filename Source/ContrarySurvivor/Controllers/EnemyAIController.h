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

	// --- QA headless-автотест погони (cs.TestWolfChase) ---
	// Аксессоры для CU-free теста: НЕ дублируют логику AI, а возвращают её реальные решения
	// (анти-галлюцинация: пороги/состояние берутся из самого контроллера, не выдумываются).

	// true, если враг СЕЙЧАС в Chase и последняя итерация погони шла по навмешу (mode=nav),
	// а не fallback-прямым ходом (mode=direct). Источник — то же решение bUseDirect в Tick.
	bool IsChaseModeNavForQA() const
	{
		return CurrentState == EEnemyAIState::Chase && !bLastChaseUsedDirect;
	}

	// Эффективная дальность атаки центр-к-центру (см) против конкретной цели: реальный
	// AttackRange (поверхность-к-поверхности) + сумма радиусов капсул врага и цели.
	// Тот же расчёт, что в Tick (EffectiveAttackRange) — это и есть «контакт достигнут».
	float GetEffectiveAttackRangeForQA(APawn* Target) const
	{
		return AttackRange + GetCombinedCapsuleRadius(Target);
	}

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

	// Выполняет атаку по игроку (урон через TakeDamage по кулдауну).
	// Возвращает true, если удар реально нанесён (не на кулдауне) — наследник (волк)
	// использует это, чтобы проиграть анимацию укуса только в момент удара.
	virtual bool PerformAttack(APawn* Player);

	// Интервал дросселирования QA-лога погони (сек). Лог пишется не чаще раза в этот период,
	// чтобы не спамить каждый тик. Только для верификации QA по логу.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Debug")
	float ChaseLogInterval = 1.0f;

	// Период переотдачи MoveToActor в Chase (сек). Пока враг в Chase и игрок в радиусе,
	// MoveToActor переотдаётся НЕПРЕРЫВНО: сразу, если path-following не в состоянии Moving
	// (завершился/зафейлился/Idle), и периодически раз в RepathInterval — чтобы цель
	// отслеживала движущегося игрока. НЕ каждый тик (вызов каждый кадр рестартит запрос
	// и мешает пешке реально двигаться).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Movement")
	float RepathInterval = 0.35f;

	// --- Fallback: прямая погоня (steering без навмеша) ---
	// На маленькой открытой карте демки навигация по навмешу ненадёжна: цель (игрок на
	// телепорт-точках) или сам враг бывают ВНЕ навмеша, либо динамический навмеш у боевой
	// зоны ещё не готов → MoveToActor отдаёт Failed и враг стоит. Тогда переключаемся на
	// прямой ход к игроку через AddMovementInput (игнорирует навмеш). В деревне, где навмеш
	// работает, остаётся обычная nav-погоня (обходит препятствия). Direct — именно fallback.

	// Сколько секунд враг должен НЕ сближаться с игроком (dist не убывает заметно), чтобы
	// счесть nav-погоню застрявшей и включить прямой ход. Защита от «навмеш есть, но путь
	// упирается / враг толчётся на месте».
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Movement")
	float StuckConvergeTime = 1.5f;

	// Минимальное убывание дистанции (см), считающееся реальным прогрессом сближения.
	// Меньше — считаем шумом, прогресс не засчитываем (иначе микродрожь сбросит таймер застревания).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Movement")
	float ChaseConvergeEpsilon = 5.0f;

private:
	// Время последней атаки (по GetWorld()->GetTimeSeconds()).
	float LastAttackTime = -1000.0f;

	// Время последнего QA-лога погони (дроссель). -1000 — чтобы первый Chase залогировался сразу.
	float LastChaseLogTime = -1000.0f;

	// Последнее решение Tick'а: шла ли погоня прямым ходом (direct, fallback) или по навмешу (nav).
	// Зеркалит bUseDirect; читается аксессором IsChaseModeNavForQA() для headless-теста погони.
	// true по умолчанию — пока враг не вошёл в Chase, «nav» не утверждаем.
	bool bLastChaseUsedDirect = true;

	// Время последней отдачи MoveToActor в Chase. -1000 — чтобы первый Chase отдал move сразу.
	float LastMoveIssueTime = -1000.0f;

	// Лучшая (минимальная) дистанция до игрока, достигнутая за текущую погоню, и время, когда
	// мы последний раз реально сблизились. Если за StuckConvergeTime сек прогресса нет —
	// nav-погоня считается застрявшей и включается прямой ход (fallback). Сбрасываются на входе в Chase.
	float BestChaseDist = 0.0f;
	float LastProgressTime = -1000.0f;

	// Результат последнего MoveToActor в Chase (для QA-диагностики навигации). Тип возвращается
	// MoveToActor (AAIController). Инициализируется в конструкторе (Failed) — в этом заголовке
	// доступна лишь форвард-декларация namespace-enum, поэтому инициализатор задаём в .cpp.
	EPathFollowingRequestResult::Type LastMoveResult;

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
};
