// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

class UStatsComponent;
class UNavigationQueryFilter;

// Состояния примитивного state-machine (MVP без Behavior Tree, ADR/tech-design Фаза 1).
// Этап D добавил: Return (возврат к дому по поводку, ADR-036), VillagePause (замер у границы
// деревни перед уходом), Standoff (ждёт поодаль — лимит одновременных атакующих, ADR-037).
UENUM(BlueprintType)
enum class EEnemyAIState : uint8
{
	Idle         UMETA(DisplayName = "Idle"),          // нет цели / цель не видна
	Chase        UMETA(DisplayName = "Chase"),         // движется к игроку
	Attack       UMETA(DisplayName = "Attack"),        // в радиусе (ближний или огнестрел), атакует с кулдауном
	Return       UMETA(DisplayName = "Return"),        // возвращается к точке дома (поводок/после паузы у деревни)
	VillagePause UMETA(DisplayName = "VillagePause"),  // стоит у границы деревни пару секунд, затем уходит
	Standoff     UMETA(DisplayName = "Standoff")       // держится поодаль: атакующие слоты заняты (ADR-037)
};

/**
 * Простой AI-контроллер врага: Idle -> Chase -> Attack (+ Return/VillagePause/Standoff, Этап D).
 * Обнаружение: дистанция + линия видимости (LineOfSightTo).
 * Движение: MoveToActor (навмеш) с fallback прямого хода. Атака: ближняя по кулдауну;
 * бандит дополнительно стреляет с дистанции (ADR-035: не стреляет, пока сам вне кадра камеры).
 * Поводок (ADR-036): дом задаёт спавнер (AMasterEnemyBase::SetLeash) либо точка спавна.
 */
UCLASS()
class CONTRARYSURVIVOR_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

	// --- Поводок (ADR-036, Этап D) ---

	// Задаёт точку дома и радиус поводка (см). Зовёт спавнер (AMasterEnemyBase) сразу после
	// спавна врага. Radius <= 0 отключает поводок. Для врагов без базы дом = позиция пешки
	// на OnPossess, радиус = DefaultLeashRadius.
	void SetLeash(const FVector& InHomeLocation, float InLeashRadius);

	// --- Опрос состояния для камеры/HUD (Этап D) ---

	// Враг сейчас «ведёт бой» с игроком (боевая камера ADR-035 считает его угрозой).
	bool IsEngagingPlayer() const
	{
		return CurrentState == EEnemyAIState::Chase
			|| CurrentState == EEnemyAIState::Attack
			|| CurrentState == EEnemyAIState::Standoff
			|| CurrentState == EEnemyAIState::VillagePause;
	}

	// Враг — стрелок, ведущий бой (HUD рисует красную краевую стрелку, если он за кадром).
	bool IsRangedThreat() const
	{
		return bRangedAttacker && IsEngagingPlayer();
	}

	// Реестр живых контроллеров врагов (для боевой камеры игрока и лимита атакующих).
	static const TArray<TWeakObjectPtr<AEnemyAIController>>& GetActiveControllers()
	{
		return ActiveControllers;
	}

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

	// --- QA-аксессоры фикса прямой видимости атаки (фикс 08-07: «убили сквозь здание») ---
	// Тонкие обёртки над РЕАЛЬНЫМИ боевыми методами (анти-галлюцинация: автотест зовёт ту же
	// логику, что бой, а не её копию). Гейт видимости стоит внутри самих Perform*Attack.

	bool HasAttackLineOfSightForQA(APawn* Target) const { return HasAttackLineOfSight(Target); }
	bool PerformAttackForQA(APawn* Target) { return PerformAttack(Target); }
	bool PerformRangedAttackForQA(APawn* Target) { return PerformRangedAttack(Target); }

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	// --- Параметры восприятия/боя (тюнингуемые) ---

	// Дистанция обнаружения игрока (см). За пределами — Idle.
	// ADR-075: EditAnywhere (было EditDefaultsOnly) — боевые числа врага настраиваются в
	// BP-наследниках контроллеров (BP_EnemyAIController/BP_WolfAIController) без правки кода.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	float DetectionRange = 1500.0f;

	// Дальность атаки ножом, измеряется ПОВЕРХНОСТЬ-К-ПОВЕРХНОСТИ капсул (см),
	// т.е. зазор между капсулами врага и игрока, а НЕ расстояние между их центрами.
	// Эффективная проверка центр-к-центру = AttackRange + (радиус капсулы врага + игрока).
	// ADR-075: EditAnywhere — тюнинг в BP контроллера (см. DetectionRange).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0.0"))
	float AttackRange = 90.0f;

	// --- Прямая видимость атаки (фикс 08-07, живой прогон Рината: «бандиты убили сквозь
	// здание»). ЛЮБОЕ нанесение урона (выстрел И ближний удар) требует свободной линии от
	// глаз атакующего до цели: линия перекрыта зданием/деревом (объект блокирует канал
	// ниже) — атака не наносится и не начинается, враг остаётся в погоне и обходит преграду
	// по навмешу. Дыра до фикса: LineOfSightTo гейтил только ВСТУПЛЕНИЕ в бой из Idle, а
	// враг, уже ведущий бой, бил по чистой дистанции + «сам в кадре» (камера сверху видит
	// его и поверх крыши). Директива Рината 06-25: тюнинг — EditAnywhere наверху Details. ---

	// Требовать прямую видимость для атак (выключатель для отладки).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat", meta = (DisplayPriority = "1"))
	bool bRequireAttackLineOfSight = true;

	// Канал трассировки линии атаки. Visibility — тот же канал, что у обнаружения игрока
	// (LineOfSightTo) и у выстрела игрока (ARangedWeapon): стены/деревья с обычной коллизией
	// его блокируют, капсулы пешек — нет (профиль Pawn игнорирует Visibility).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat", meta = (DisplayPriority = "2"))
	TEnumAsByte<ECollisionChannel> AttackLineOfSightChannel = ECC_Visibility;

	// Радиус сферы трассировки (см). 0 = тонкий луч (дефолт). Радиус > 0 дополнительно
	// запрещает «прострел» сквозь щель, в которую луч проскочил бы, а пуля осмысленно нет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Combat", meta = (ClampMin = "0.0", DisplayPriority = "3"))
	float AttackLineOfSightRadius = 0.0f;

	// Радиус приёмки для MoveToActor (см). Останавливаемся, не упираясь в игрока.
	// Должен быть таким, чтобы дистанция остановки преследования была <= дальности атаки
	// (с учётом радиусов капсул обоих). 60 < AttackRange(90) поверхность-к-поверхности.
	// ADR-075: EditAnywhere — тюнинг в BP контроллера (см. DetectionRange).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Movement", meta = (ClampMin = "0.0"))
	float MoveAcceptanceRadius = 60.0f;

	// Навигационный фильтр поиска пути врага (BugReport 12): ИСКЛЮЧАЕТ зону деревни
	// (UNavArea_Village) — бандиты и волки строят путь В ОБХОД деревни. Дефолт задаётся в
	// конструкторе = UNavQueryFilter_ExcludeVillage. Передаётся в каждый MoveToActor.
	// Нейтралы этот контроллер не используют → ходят нормально (без фильтра).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Movement")
	TSubclassOf<UNavigationQueryFilter> MoveFilterClass;

	// Урон одной атаки (ближний удар). У волка это урон укуса (дефолт 10 задаёт конструктор
	// AWolfAIController). ADR-075: EditAnywhere — тюнинг в BP контроллера.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0.0"))
	float AttackDamage = 10.0f;

	// Кулдаун между атаками (сек). ADR-075: EditAnywhere — тюнинг в BP контроллера.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AI|Combat", meta = (ClampMin = "0.05"))
	float AttackCooldown = 1.5f;

	// --- Огнестрел (D6, ADR-035). Бандит стреляет с дистанции; в упор остаётся ближний удар.
	// Волк (AWolfAIController) выключает bRangedAttacker в конструкторе. Все числа DRAFT,
	// соизмерены с HP игрока (100): урон 12 = укус волка; при текущей броне игрока
	// (кап 0.75) эффективно ~3 HP/попадание. Директива Рината 06-25: EditAnywhere+BRW+наверх.

	// Есть ли у врага огнестрел (стрельба с дистанции). Визуал — пистолет в руке (AEnemyCharacter).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Ranged", meta = (DisplayPriority = "1"))
	bool bRangedAttacker = true;

	// Максимальная дистанция выстрела центр-к-центру (см). Дальше — сближается (Chase).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Ranged", meta = (ClampMin = "0.0", DisplayPriority = "2"))
	float RangedAttackRange = 1100.0f;

	// Урон одного попадания.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Ranged", meta = (ClampMin = "0.0", DisplayPriority = "3"))
	float RangedAttackDamage = 12.0f;

	// Кулдаун между выстрелами (сек).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Ranged", meta = (ClampMin = "0.05", DisplayPriority = "4"))
	float RangedAttackCooldown = 1.6f;

	// Шанс попадания [0..1] (разброс как вероятность, дешевле честной баллистики).
	// Промах уводит след пули мимо игрока (RangedMissOffset).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Ranged", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "5"))
	float RangedHitChance = 0.7f;

	// Насколько (см) промах уходит вбок от игрока (визуал следа пули мимо).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Ranged", meta = (ClampMin = "0.0", DisplayPriority = "6"))
	float RangedMissOffset = 140.0f;

	// «Правило честности» (ADR-035): враг НЕ стреляет, пока он сам вне кадра камеры игрока.
	// Вне кадра он продолжает сближаться, HUD ведёт на него красную краевую стрелку.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Ranged", meta = (DisplayPriority = "7"))
	bool bHonorCameraFairness = true;

	// --- Поводок (D7, ADR-036) ---

	// Радиус поводка для врага БЕЗ базы (см): дом = позиция на OnPossess. Врагам от базы
	// радиус задаёт спавнер (AMasterEnemyBase::LeashRadius -> SetLeash). 0 = поводок выкл.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Leash", meta = (ClampMin = "0.0", DisplayPriority = "8"))
	float DefaultLeashRadius = 4000.0f;

	// Радиус приёмки возврата домой (см): ближе — считаем «дома», снова Idle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Leash", meta = (ClampMin = "50.0", DisplayPriority = "9"))
	float ReturnAcceptRadius = 250.0f;

	// Если игрок потерян (не обнаружен), а враг дальше этого от дома — идёт домой, не стоит
	// посреди карты (ADR-036 «уходят обратно»). 0 = не возвращаться (стоит где потерял).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Leash", meta = (ClampMin = "0.0", DisplayPriority = "10"))
	float ReturnHomeWhenIdleBeyond = 800.0f;

	// --- Деревня — безопасная зона (D7, ADR-036). Границу даёт AVillageZone (оба режима погони). ---

	// Соблюдать запрет на вход в деревню (выключатель для отладки).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Village", meta = (DisplayPriority = "11"))
	bool bRespectVillageSafeZone = true;

	// На сколько см вперёд по ходу движения проверяется граница деревни: точка-впереди в
	// зоне → стоп у границы (VillagePause). Работает и в направлении прямой погони (fallback).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Village", meta = (ClampMin = "0.0", DisplayPriority = "12"))
	float VillageStopLookAhead = 250.0f;

	// Дистанция (см) впереди, с которой враг начинает ЗАМЕДЛЯТЬСЯ, приближаясь к границе деревни.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Village", meta = (ClampMin = "0.0", DisplayPriority = "13"))
	float VillageSlowdownLookAhead = 700.0f;

	// Множитель скорости при замедлении у границы деревни [0..1].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Village", meta = (ClampMin = "0.05", ClampMax = "1.0", DisplayPriority = "14"))
	float VillageSlowdownSpeedFactor = 0.45f;

	// Сколько секунд враг стоит у границы деревни, «глядя» на игрока, прежде чем уйти домой.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Village", meta = (ClampMin = "0.0", DisplayPriority = "15"))
	float VillagePauseSeconds = 2.0f;

	// --- Плотность боя (D7, ADR-037: «≤2-3 активных врага одновременно») ---

	// Сколько врагов МАКСИМУМ одновременно ведут бой (Chase/Attack). Ближайшие к игроку
	// занимают слоты; остальные — Standoff: стоят поодаль, смотрят на игрока и атакуют,
	// только если игрок сам подошёл вплотную (решение game-lead, Этап D).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Density", meta = (ClampMin = "1", DisplayPriority = "16"))
	int32 MaxSimultaneousAttackers = 3;

	// Текущее состояние (для отладки/привязок).
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "AI|State")
	EEnemyAIState CurrentState = EEnemyAIState::Idle;

	// Выполняет ближнюю атаку по игроку (урон через TakeDamage по кулдауну).
	// Возвращает true, если удар реально нанесён (не на кулдауне) — наследник (волк)
	// использует это, чтобы проиграть анимацию укуса только в момент удара.
	virtual bool PerformAttack(APawn* Player);

	// Выстрел по игроку (D6): кулдаун + правило честности (в кадре ли враг) + шанс попадания.
	// Урон штатным TakeDamage; след пули/вспышка/звук — через ARangedWeapon::PlayFireVisuals
	// пистолета в руке пешки. Возвращает true, если выстрел реально произошёл.
	virtual bool PerformRangedAttack(APawn* Player);

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
	// Поднято с 5 до 50: малый порог (5 см) = уровень шума, при кружащемся игроке
	// дистанция не падала даже на 5 см → ложный direct-fallback → рывки. 50 см = ~полметра
	// реального сближения, шум не триггерит застревание.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Movement")
	float ChaseConvergeEpsilon = 50.0f;

	// --- Хелперы Этапа D (доступны наследнику-волку) ---

	// Перевод в состояние возврата домой (стоп, снять фокус). Причина — в QA-лог.
	void StartReturnHome(const TCHAR* Reason);

	// Тик возврата домой: MoveToLocation по навмешу, при Failed — прямой ход. Дома → Idle.
	void TickReturnHome(float Now);

	// Точка-впереди пешки по направлению Dir на дистанции Dist — внутри деревни?
	bool IsAheadInVillage(const FVector& Dir, float Dist) const;

	// Врагу доступен атакующий слот? (ADR-037: не больше MaxSimultaneousAttackers ближайших
	// к игроку врагов одновременно в Chase/Attack; остальные — Standoff.)
	bool HasAttackSlot(APawn* Player) const;

	// Замедление у границы деревни: true → MaxWalkSpeed * VillageSlowdownSpeedFactor,
	// false → восстановить базовую скорость пешки (кэш CachedBaseWalkSpeed).
	void SetVillageSlowdown(bool bSlow);

	// Враг в кадре камеры игрока? (проекция позиции пешки на экран + границы вьюпорта).
	bool IsSelfOnPlayerScreen() const;

private:
	// Время последней ближней атаки (по GetWorld()->GetTimeSeconds()).
	float LastAttackTime = -1000.0f;

	// Время последнего выстрела (огнестрел, D6).
	float LastRangedAttackTime = -1000.0f;

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

	// --- Рантайм поводка/деревни/замедления (Этап D) ---

	// Точка дома (центр базы от спавнера либо позиция на OnPossess) и эффективный радиус поводка.
	FVector HomeLocation = FVector::ZeroVector;
	float LeashRadius = 0.0f;
	bool bLeashSet = false;

	// Момент окончания паузы у границы деревни (GetTimeSeconds).
	float VillagePauseEndTime = 0.0f;

	// Кэш базовой MaxWalkSpeed пешки (ленивая инициализация в Tick: к первому тику пешка
	// уже применила свою скорость в BeginPlay). <0 = ещё не кэширована.
	float CachedBaseWalkSpeed = -1.0f;

	// Замедление у деревни сейчас активно (чтобы не переписывать MaxWalkSpeed каждый тик).
	bool bVillageSlowdownActive = false;

	// Кэш статов своего пешки (для проверки "враг жив").
	UPROPERTY()
	UStatsComponent* OwnStats = nullptr;

	// Реестр живых контроллеров врагов (лимит атакующих ADR-037 + боевая камера ADR-035).
	static TArray<TWeakObjectPtr<AEnemyAIController>> ActiveControllers;

	// Находит игрока-пешку (через PlayerController 0).
	APawn* GetPlayerPawn() const;

	// Видит ли врага игрока: дистанция в пределах DetectionRange + LineOfSightTo.
	bool CanSensePlayer(APawn* Player) const;

	// Свободна ли линия атаки: трассировка канала AttackLineOfSightChannel от глаз пешки
	// (GetPawnViewLocation) до центра цели, сферой радиуса AttackLineOfSightRadius (0 = луч).
	// Игнорирует обе пешки и прикреплённые к ним акторы (оружие в руках). При выключенном
	// bRequireAttackLineOfSight всегда true.
	bool HasAttackLineOfSight(APawn* Target) const;

	// Сумма радиусов капсул врага и игрока (см). Нужна, чтобы дистанции боя/остановки
	// мерить поверхность-к-поверхности, а не центр-к-центру (GetDistanceTo даёт центры).
	float GetCombinedCapsuleRadius(APawn* Player) const;
};
