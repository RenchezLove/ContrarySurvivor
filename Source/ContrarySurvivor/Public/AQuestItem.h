// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AMasterInventoryItem.h"
#include "AQuestItem.generated.h"

/**
 * Квест-предмет (Фаза 5, GDD §7.7): «Шкура волка», «Ноутбук» и т.п.
 *
 * Категория = Quest (EItemCategory::Quest). Следствия:
 *  - НЕ теряется при смерти игрока (штраф смерти роняет мешком только Consumable —
 *    APlayerCharacter::DropConsumablesAsBag, ADR-027);
 *  - Use() — намеренно пустой (квест-предмет нельзя «съесть»/применить из рюкзака);
 *  - изымается из инвентаря при сдаче квеста старосте (UQuestComponent::TurnInQuest).
 *
 * Конкретный класс (не Abstract) — можно спавнить из C++ без BP (дроп волка, спавн ноутбука
 * подсистемой базы, debug-выдача). Имя предмета задаётся ItemName на заспавненном экземпляре.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AQuestItem : public AMasterInventoryItem
{
	GENERATED_BODY()

public:
	AQuestItem();

	// Квест-предмет не используется из рюкзака (в отличие от расходника). Переопределяем,
	// чтобы базовый Use() ничего не делал (нет эффекта/уничтожения).
	virtual void Use() override;
};
