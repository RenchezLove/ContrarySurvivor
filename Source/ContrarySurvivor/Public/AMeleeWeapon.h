// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AMasterWeapon.h"
#include "AMeleeWeapon.generated.h"

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
 * ЧЕРНОВЫЕ ЧИСЛА (draft, на ревью game-lead/Рината): урон 35, кулдаун 1.0с,
 * дальность короткая (MeleeRange 90 поверхность-к-поверхности).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AMeleeWeapon : public AMasterWeapon
{
	GENERATED_BODY()

public:
	AMeleeWeapon();

	// Атака ножом: overlap-сфера вокруг носителя, урон целям-пешкам в коротком радиусе.
	// Target игнорируется (атака радиальная — для top-down надёжнее, чем направленный конус,
	// т.к. персонаж не обязательно повёрнут к врагу). Кулдаун замаха.
	virtual void Fire(AActor* Target) override;

protected:
	// Дальность атаки ПОВЕРХНОСТЬ-К-ПОВЕРХНОСТИ капсул (см). Эффективная проверка
	// центр-к-центру = MeleeRange + (радиус капсулы носителя + радиус капсулы цели).
	// DRAFT.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee")
	float MeleeRange = 90.0f;

	// Кулдаун замаха (сек) между атаками ножом. DRAFT.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Melee")
	float AttackCooldown = 1.0f;

private:
	// Время последней атаки (GetWorld()->GetTimeSeconds()).
	float LastMeleeTime = -1000.0f;
};
