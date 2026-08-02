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
	// Water -> DrinkWater (+Thirst), Medkit -> Heal.
	// ВОЗВРАТ (Build 1.2.1, стаки): true — предмет израсходован ЦЕЛИКОМ, вызывающий убирает
	// актор из рюкзака и уничтожает (прежнее поведение); false — либо эффект не применён,
	// либо съедена ОДНА штука из стака (счётчик уменьшен, актор живёт дальше).
	UFUNCTION(BlueprintCallable, Category = "Consumable")
	bool ApplyConsumeEffect(UStatsComponent* Stats);

	// СЛУЖЕБНЫЙ КЛЮЧ расходника по типу — идёт в AMasterInventoryItem::ItemName, по нему
	// сходится логика квестов. НЕ переводится, значения не менять (ADR-050, порция 0).
	static FString GetDefaultDisplayName(EConsumableType Type);

	// ПЕРЕВОДИМОЕ название того же расходника, которое видит игрок («Консервы»/«Вода»/
	// «Бинт»). Единственное место этих слов в коде: и лут бандита, и каталог торговца,
	// и отладочная выдача берут название отсюда.
	static FText GetDefaultDisplayText(EConsumableType Type);

	// Иконка расходника по типу (Build 1.2.2, тайлы): рендеры модельера T_Item_*.
	// Статик — чтобы каталог торговца мог показать иконку товара ДО спавна предмета.
	static TSoftObjectPtr<UTexture2D> GetDefaultIcon(EConsumableType Type);

	// Один класс обслуживает еду/воду/аптечку, а тип выставляется ПОСЛЕ спавна (лут
	// бандита, покупка) — иконка вычисляется по текущему типу, явный ItemIcon главнее.
	virtual TSoftObjectPtr<UTexture2D> GetItemIcon() const override;
};
