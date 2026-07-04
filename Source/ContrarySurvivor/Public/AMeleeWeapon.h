// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AMasterWeapon.h"
#include "Engine/TimerHandle.h"
#include "AMeleeWeapon.generated.h"

class USoundBase;

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
 * ВПЕРЕДИ носителя (полуугол MeleeSectorHalfAngleDeg), НЕ круговой 360° урон. Перед взмахом
 * носитель-ИГРОК доворачивается к залоченной цели в радиусе (bTurnToLockedTarget, решение
 * game-lead) — сектор остаётся передним, но лок не промахивается из-за ориентации бега.
 * Плюс микро-заморозка (hitstop) при реальном попадании игрока (D5).
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

	// Атака ножом (ADR-037): до MaxTargetsPerSwing ближайших целей в ПЕРЕДНЕМ секторе
	// (полуугол MeleeSectorHalfAngleDeg) на дистанции MeleeRange. Target игнорируется —
	// приоритет лока реализован доворотом носителя (bTurnToLockedTarget). Кулдаун замаха.
	virtual void Fire(AActor* Target) override;

protected:
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
	// 60° = конус 120° впереди. «Если нож слаб против 2-3 — расширить сектор, НЕ делать круг».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (ClampMin = "5.0", ClampMax = "180.0", DisplayPriority = "3"))
	float MeleeSectorHalfAngleDeg = 60.0f;

	// Максимум целей за один взмах (ADR-037: «1-2 цели впереди, несколько за удар — ок»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (ClampMin = "1", DisplayPriority = "4"))
	int32 MaxTargetsPerSwing = 2;

	// Доворачивать носителя-ИГРОКА лицом к залоченной цели перед взмахом, если она в радиусе
	// (решение game-lead: стандарт top-down; сектор остаётся передним — ADR-037 не нарушен).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Melee", meta = (DisplayPriority = "5"))
	bool bTurnToLockedTarget = true;

	// --- Микро-заморозка при попадании (hitstop, D5) ---

	// Включатель hitstop (только для попаданий ИГРОКА, не ИИ).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|HitStop", meta = (DisplayPriority = "6"))
	bool bEnableHitStop = true;

	// Замедление времени на время hitstop (global time dilation). 0.05 = почти стоп-кадр.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|HitStop", meta = (ClampMin = "0.01", ClampMax = "1.0", DisplayPriority = "7"))
	float HitStopTimeDilation = 0.05f;

	// Длительность hitstop в секундах РЕАЛЬНОГО времени.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|HitStop", meta = (ClampMin = "0.01", ClampMax = "0.5", DisplayPriority = "8"))
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
	// Время последней атаки (GetWorld()->GetTimeSeconds()).
	float LastMeleeTime = -1000.0f;

	// Микро-заморозка (D5): SetGlobalTimeDilation(HitStopTimeDilation) + таймер восстановления.
	// Таймер идёт в ИГРОВОМ времени, поэтому его период = HitStopDuration * dilation
	// (реальное время паузы = HitStopDuration).
	void ApplyHitStop();

	FTimerHandle HitStopTimerHandle;
};
