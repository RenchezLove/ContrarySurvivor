// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CorpseLootComponent.generated.h"

class AMasterInventoryItem;

/**
 * Контейнер лута ТРУПА врага (Build 1.2.1, ТЗ А1 «Обыск трупов»).
 *
 * При смерти враг больше не роняет мешок-пикап: деньги и предметы складываются СЮДА
 * (InitLoot из HandleDeath/DropLoot), труп становится «обыскиваемым» — контроллер
 * показывает подсказку «Обыскать [E]» и по E открывает окно UCorpseLootWidget.
 * Частичный обыск штатен: забранное уходит игроку, остаток живёт в компоненте до
 * исчезновения трупа (SetLifeSpan владельца, CorpseLifeSpan врага).
 *
 * Компонент создаётся в КОНСТРУКТОРАХ мастер-классов врагов (AEnemyCharacter,
 * AWolfCharacter) — BP-наследники получают механизм автоматически (ТЗ А1). Мешок
 * смерти ИГРОКА остаётся пикапом как есть — на игроке этого компонента нет.
 *
 * Находимость для контроллера — статический реестр «обыскиваемых трупов» (паттерн
 * AEnemyAIController::GetActiveControllers): регистрация в InitLoot (труп появился),
 * снятие в EndPlay (труп исчез по таймеру / конец мира). Предметы-лут — скрытые
 * акторы-данные (как в APickup::InitLootBag); не забранные к EndPlay уничтожаются.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class CONTRARYSURVIVOR_API UCorpseLootComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCorpseLootComponent();

	// Положить лут в труп (зовётся ОДИН раз из HandleDeath врага). Регистрирует труп
	// в реестре обыскиваемых. Items — скрытые акторы-данные (владение переходит сюда).
	void InitLoot(float InMoney, const TArray<AMasterInventoryItem*>& InItems);

	// Есть ли что забирать (деньги или хоть один живой предмет).
	UFUNCTION(BlueprintPure, Category = "CorpseLoot")
	bool HasLoot() const;

	UFUNCTION(BlueprintPure, Category = "CorpseLoot")
	float GetMoney() const { return Money; }

	// Живые (валидные) предметы трупа — для списка окна обыска.
	TArray<AMasterInventoryItem*> GetLootItems() const;

	// Забрать деньги: возвращает сумму и обнуляет её в трупе.
	float TakeMoney();

	// Забрать предмет: убирает его из списка трупа (true — предмет был здесь).
	// Сам актор НЕ уничтожается — его дальше несёт инвентарь игрока.
	bool TakeItem(AMasterInventoryItem* Item);

	// Реестр обыскиваемых трупов (для UpdateNearbyInteractable контроллера).
	// Слабые ссылки: труп исчезает по таймеру — запись отмирает сама, но чистим в EndPlay.
	static const TArray<TWeakObjectPtr<UCorpseLootComponent>>& GetSearchableCorpses()
	{
		return SearchableCorpses;
	}

protected:
	// Труп исчез (LifeSpan/конец мира): снять с реестра, уничтожить не забранные предметы.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// Деньги в трупе (забираются одной кнопкой строки «Деньги»).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CorpseLoot", meta = (AllowPrivateAccess = "true"))
	float Money = 0.0f;

	// Предметы в трупе (скрытые акторы-данные; UPROPERTY — защита от GC до забора).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CorpseLoot", meta = (AllowPrivateAccess = "true"))
	TArray<TObjectPtr<AMasterInventoryItem>> Items;

	static TArray<TWeakObjectPtr<UCorpseLootComponent>> SearchableCorpses;
};
