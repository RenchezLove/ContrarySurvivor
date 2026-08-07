// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AMasterWeapon.h"
#include "Engine/TimerHandle.h"
#include "UObject/SoftObjectPtr.h" // мягкая ссылка на монтаж замаха
#include "AMeleeWeapon.generated.h"

class USoundBase;
class UAnimMontage;

/**
 * Ближнее оружие (нож) — GDD §7.2: «атака по цели в коротком радиусе (sweep/overlap),
 * урон, кулдаун замаха. Без комбо.»
 *
 * Конкретный класс (НЕ Abstract): единственное ближнее оружие ростера MVP = нож,
 * поэтому статы ножа заданы прямо в конструкторе (можно переопределить в BP-наследнике).
 *
 * Атака переиспользует виртуальный AMasterWeapon::Fire(AActor*) — игрок бьёт ножом через
 * тот же путь ввода, что и стрельбу (полиморфизм). Радиус короткий, измеряется
 * поверхность-к-поверхности капсул (как в фиксе боя бандита, Фаза 2).
 *
 * ЭТАП D (ADR-037): удар = ПЕРЕДНИЙ ВЗМАХ/СЕКТОР — задевает до MaxTargetsPerSwing целей
 * ВПЕРЕДИ носителя (полуугол MeleeSectorHalfAngleDeg), НЕ круговой 360° урон. При взмахе
 * носитель-ИГРОК доворачивается к залоченной цели в радиусе (bTurnToLockedTarget). С Build 1.1
 * доворот ПЛАВНЫЙ (тот же StartAimTurnTo, что при стрельбе): текущий взмах считается по
 * направлению взгляда НА МОМЕНТ УДАРА, поэтому бьёт ровно туда, где нарисован подсвеченный
 * сектор (UMeleeSectorIndicatorComponent). Прежний мгновенный рывок остался под флагом
 * bMeleeSnapToTarget. Плюс микро-заморозка (hitstop) при реальном попадании игрока (D5).
 *
 * ЧЕРНОВЫЕ ЧИСЛА (draft, на ревью game-lead/Рината): урон 35, кулдаун 1.0с,
 * дальность короткая (MeleeRange 90 поверхность-к-поверхности).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AMeleeWeapon : public AMasterWeapon
{
	GENERATED_BODY()

public:
	AMeleeWeapon();

	// Замах ножом: кулдаун, звук, доворот к цели и запуск анимации замаха. Сам УРОН наносится
	// не здесь, а по кадру взмаха — уведомлением UAnimNotify_MeleeHit на дорожке монтажа
	// (Build 1.1). Анимации нет — урон наносится сразу, как раньше. Target игнорируется:
	// цели выбирает сектор.
	virtual void Fire(AActor* Target) override;

	// Урон по ПЕРЕДНЕМУ СЕКТОРУ (ADR-037): до MaxTargetsPerSwing ближайших целей в секторе
	// с полууглом MeleeSectorHalfAngleDeg на дистанции MeleeRange, плюс микро-заморозка при
	// попадании игрока. Зовётся уведомлением UAnimNotify_MeleeHit с кадра взмаха, а при
	// отсутствии анимации — прямо из Fire(). Публичный и BlueprintCallable, потому что точку
	// вызова задаёт метка на дорожке анимации, а не код оружия.
	UFUNCTION(BlueprintCallable, Category = "Weapon|Melee")
	void ApplyMeleeDamage();

	// Ждёт ли ЭТО оружие свою метку урона: замах начат через Fire() и анимация реально пошла.
	// Защита от двойного урона: ту же анимацию удара крутит и бандит, но его урон считает ИИ
	// отдельно (AEnemyAIController::PerformAttack), через оружие он не бьёт. Поэтому метка
	// наносит урон ТОЛЬКО когда взмах начало само оружие. Флаг снимается первой же меткой.
	bool ConsumePendingSwing();

	// Страховка hitstop (qa): если оружие уничтожают в окно замедления (~HitStopDuration),
	// таймер восстановления (WeakLambda) уже не сработает и global time dilation залип бы
	// НАВСЕГДА — восстанавливаем дилатацию здесь.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- Геометрия удара наружу (Build 1.1) ---
	// Читает UMeleeSectorIndicatorComponent, чтобы подсветка сектора на земле рисовалась
	// ровно по тем числам, по которым считается урон в Fire(): «что видишь, то и бьёшь».
	// Своих чисел угла/дальности у подсветки нет — только эти два.

	UFUNCTION(BlueprintPure, Category = "Weapon|Melee")
	FORCEINLINE float GetMeleeSectorHalfAngleDeg() const { return MeleeSectorHalfAngleDeg; }

	UFUNCTION(BlueprintPure, Category = "Weapon|Melee")
	FORCEINLINE float GetMeleeRange() const { return MeleeRange; }

protected:
	// --- Прямая видимость удара (фикс 08-07: «сквозь стены бить нельзя — никому») ---
	// Кандидат попал в сектор по дистанции и углу, но между носителем и ним стена/дерево
	// (объект блокирует канал ниже) — удар по нему НЕ засчитывается. Дыра до фикса: сектор
	// выбирался чистым overlap'ом, тонкая преграда в упор от ножа не спасала.

	// Требовать прямую видимость до цели удара (выключатель для отладки).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (DisplayPriority = "0"))
	bool bRequireLineOfSight = true;

	// Канал трассировки видимости удара (Visibility — тот же, что у выстрелов и сенсинга ИИ).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (DisplayPriority = "0"))
	TEnumAsByte<ECollisionChannel> LineOfSightChannel = ECC_Visibility;

	// Дальность атаки ПОВЕРХНОСТЬ-К-ПОВЕРХНОСТИ капсул (см). Эффективная проверка
	// центр-к-центру = MeleeRange + (радиус капсулы носителя + радиус капсулы цели).
	// DRAFT. Директива Рината 06-25: тюнинг-параметры EditAnywhere + наверх Details.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (ClampMin = "0.0", DisplayPriority = "1"))
	float MeleeRange = 90.0f;

	// Кулдаун замаха (сек) между атаками ножом. DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (ClampMin = "0.05", DisplayPriority = "2"))
	float AttackCooldown = 1.0f;

	// --- Передний сектор (D3, ADR-037) ---

	// Полуугол переднего сектора удара (градусы от направления взгляда носителя).
	// 50° = конус 100° впереди — требование Рината (Build 1.1, п.5: «подсвечивать сектор
	// в 100 градусов перед игроком», в этом же секторе наносится урон).
	// «Если нож слаб против 2-3 — расширить сектор, НЕ делать круг».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (ClampMin = "5.0", ClampMax = "180.0", DisplayPriority = "3"))
	float MeleeSectorHalfAngleDeg = 50.0f;

	// Максимум целей за один взмах (ADR-037: «1-2 цели впереди, несколько за удар — ок»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (ClampMin = "1", DisplayPriority = "4"))
	int32 MaxTargetsPerSwing = 2;

	// Доворачивать носителя-ИГРОКА к залоченной цели при взмахе, если она в радиусе
	// (решение game-lead: стандарт top-down; сектор остаётся передним — ADR-037 не нарушен).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (DisplayPriority = "5"))
	bool bTurnToLockedTarget = true;

	// КАК доворачивать (Build 1.1, решение game-lead по дословной формулировке Рината:
	// «в ЭТОМ ЖЕ секторе должен наноситься урон»).
	// Выключено (по умолчанию) — доворот ПЛАВНЫЙ, тем же механизмом, что при стрельбе
	// (AMasterHumanoidCharacter::StartAimTurnTo): корпус едет к цели за несколько кадров, а
	// ТЕКУЩИЙ взмах считается по тому направлению, куда игрок смотрит СЕЙЧАС — то есть ровно
	// по подсвеченному на земле сектору. Нож может промахнуться, если игрок смотрит мимо.
	// Включено — прежний мгновенный рывок лицом к цели прямо перед проверкой сектора: нож
	// почти не мажет, но бьёт туда, куда игрок ещё не успел посмотреть, и подсветка расходится
	// с фактическим ударом.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (DisplayName = "Мгновенный доворот при ударе", DisplayPriority = "6"))
	bool bMeleeSnapToTarget = false;

	// --- Микро-заморозка при попадании (hitstop, D5) ---

	// Включатель hitstop (только для попаданий ИГРОКА, не ИИ).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|HitStop", meta = (DisplayPriority = "7"))
	bool bEnableHitStop = true;

	// Замедление времени на время hitstop (global time dilation). 0.05 = почти стоп-кадр.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|HitStop", meta = (ClampMin = "0.01", ClampMax = "1.0", DisplayPriority = "8"))
	float HitStopTimeDilation = 0.05f;

	// Длительность hitstop в секундах РЕАЛЬНОГО времени.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|HitStop", meta = (ClampMin = "0.01", ClampMax = "0.5", DisplayPriority = "9"))
	float HitStopDuration = 0.06f;

	// --- Звук замаха ножом (Демо) ---
	// Проигрывается при каждом реальном замахе (после прохождения кулдауна, до проверки
	// попадания) — звучит и при промахе. Дефолт из /Game/Audio/Demo/knife_melee_swing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio")
	USoundBase* SwingSound;

	// Громкость замаха. Тюнингуется.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Audio", meta = (ClampMin = "0.0"))
	float SwingSoundVolume = 0.5f;

private:
	// Свободна ли линия удара от носителя до цели (фикс 08-07, см. bRequireLineOfSight).
	// Игнорирует носителя, само оружие, цель и прикреплённые к обоим акторы (оружие в руках).
	bool HasLineOfSightToTarget(const APawn* Wielder, const AActor* Target) const;

	// Дистанция ПОВЕРХНОСТЬ-К-ПОВЕРХНОСТИ от носителя до цели (центр-к-центру минус радиусы
	// капсул обоих). Одна формула на всё оружие: по ней же считает радиус подсветки сектора
	// UMeleeSectorIndicatorComponent — «что видишь, то и бьёшь». Цель невалидна или это сам
	// носитель — очень большое число (заведомо вне дальности).
	float GetSurfaceDistanceTo(const AActor* Target) const;

	// Время последней атаки (GetWorld()->GetTimeSeconds()).
	float LastMeleeTime = -1000.0f;

	// Взмах начат и ждёт свою метку урона (см. ConsumePendingSwing).
	bool bSwingAwaitingNotify = false;

	// Микро-заморозка (D5): SetGlobalTimeDilation(HitStopTimeDilation) + таймер восстановления.
	// Таймер идёт в ИГРОВОМ времени, поэтому его период = HitStopDuration * dilation
	// (реальное время паузы = HitStopDuration).
	void ApplyHitStop();

	FTimerHandle HitStopTimerHandle;

	// Замедление времени сейчас активно (выставлен ApplyHitStop, ещё не восстановлен таймером).
	// По нему EndPlay понимает, что дилатацию нужно вернуть к 1.0 (см. комментарий у EndPlay).
	bool bHitStopPending = false;
};
