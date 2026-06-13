// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AMasterInventoryItem.h"
#include "AConsumableItem.generated.h"

class UStatsComponent;

// Тип расходника (Фаза 4). Определяет, какой стат восстанавливает предмет при
// использовании из инвентаря (GDD §7.3: еда +Hunger, вода +Thirst).
UENUM(BlueprintType)
enum class EConsumableType : uint8
{
	Food   UMETA(DisplayName = "Food"),   // +Hunger (Stats->ConsumeFood, +30 из Фазы 2)
	Water  UMETA(DisplayName = "Water"),  // +Thirst (Stats->DrinkWater,  +40 из Фазы 2)
	Medkit UMETA(DisplayName = "Medkit")  // +HP (Stats->Heal) — бинт/аптечка (GDD §7.6)
};

/**
 * Расходуемый предмет (еда/вода). Категория = Consumable. Использование из UI-инвентаря
 * восстанавливает голод/жажду через UStatsComponent владельца и удаляет предмет из рюкзака.
 * Конкретный класс (не Abstract) — можно спавнить из C++ без BP для тестовых предметов.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AConsumableItem : public AMasterInventoryItem
{
	GENERATED_BODY()

public:
	AConsumableItem();

	// Что восстанавливает (еда -> голод, вода -> жажда, аптечка -> HP).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable")
	EConsumableType ConsumableType = EConsumableType::Food;

	// Сколько HP восстанавливает аптечка/бинт (тип Medkit). DRAFT, тюнингуется.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable")
	float HealRestoreAmount = 25.0f;

	// Применяет эффект расходника к статам потребителя. Food -> ConsumeFood (+Hunger),
	// Water -> DrinkWater (+Thirst). Возвращает true, если эффект применён.
	UFUNCTION(BlueprintCallable, Category = "Consumable")
	bool ApplyConsumeEffect(UStatsComponent* Stats);
};
