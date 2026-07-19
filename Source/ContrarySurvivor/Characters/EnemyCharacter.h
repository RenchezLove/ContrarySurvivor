// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MasterHumanoidCharacter.h"
#include "AConsumableItem.h" // EConsumableType (тип расходника в таблице лута)
#include "EnemyCharacter.generated.h"

class UStatsComponent;
class AMasterInventoryItem;
class APickup;

/**
 * Позиция таблицы лута бандита (этап F, решение Рината 07-17: «из бандита должны с равной
 * степенью вероятности выпадать консервы, вода, бинт и т.д.»). При удаче LootItemDropChance
 * падает LootItemCountMin..Max предметов, КАЖДЫЙ выбирается из таблицы равновероятно.
 * Дефолт (Консервы/Вода/Бинт) собирает конструктор AEnemyCharacter; расширяется в Details
 * без правок кода.
 */
USTRUCT(BlueprintType)
struct FBanditLootEntry
{
	GENERATED_BODY()

	// СЛУЖЕБНЫЙ КЛЮЧ предмета. Пусто -> ключ по типу расходника
	// (AConsumableItem::GetDefaultDisplayName). НЕ переводится (ADR-050, порция 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FString DisplayName;

	// ПЕРЕВОДИМОЕ название, которое видит игрок (подбор/рюкзак). Пусто -> название по типу
	// расходника (AConsumableItem::GetDefaultDisplayText).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	FText DisplayText;

	// Класс выпадающего предмета (nullptr -> AConsumableItem).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	TSubclassOf<AMasterInventoryItem> ItemClass;

	// Что восстанавливает предмет (еда/вода/аптечка); применяется, только если класс —
	// расходник (AConsumableItem или наследник).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
	EConsumableType ConsumableType = EConsumableType::Food;
};

/**
 * Враг первого вертикального среза (бандит).
 * Наследует AMasterHumanoidCharacter КАК ЕСТЬ (ADR-018: модульность остаётся в базе).
 * Несёт UStatsComponent (ADR-015) — здоровье и смерть идут через него, минуя инлайн-Health базы.
 * AI управляет AEnemyAIController (назначается через AutoPossessAI/BP).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AEnemyCharacter : public AMasterHumanoidCharacter
{
	GENERATED_BODY()

public:
	AEnemyCharacter();

protected:
	virtual void BeginPlay() override;

	// ПРАВКА B: связываем модульные части (Torso/Legs) с Head через Leader Pose,
	// чтобы они следовали позе/анимации Head как единое тело.
	virtual void PostInitializeComponents() override;

	// Компонент статов (Health/смерть). Источник истины по HP для врага.
	// meta DisplayPriority — поднять наши настройки наверх Details (фидбек Рината), сразу после Transform.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats", meta = (AllowPrivateAccess = "true", DisplayPriority = "1"))
	UStatsComponent* Stats;

	// Стартовое здоровье бандита. Черновое тюнингуемое значение.
	// 80 HP = 4 попадания из пистолета (25 урона/выстрел, 2 выстр/с) ≈ 2 сек огня. Тюнится.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats")
	float BanditMaxHealth = 80.0f;

	// D1/D6: оружие в руке бандита (визуал огнестрела ADR-035). Спавнится и экипируется в
	// BeginPlay через штатный EquipWeapon (кость R_Hand, как у игрока). Дефолт APistol
	// задаётся в конструкторе — другого огнестрела в проекте нет. nullptr = бандит без
	// оружия в руке (стрельба ИИ при этом идёт без визуала — контроллер громко логирует).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment", meta = (DisplayPriority = "2"))
	TSubclassOf<AMasterWeapon> SidearmWeaponClass;

	// Спавнит SidearmWeaponClass и экипирует в руку (виден пистолет). Зовётся в BeginPlay.
	void EquipSidearm();

	// Скорость погони бандита (см/с). TUNING. Чуть ВЫШЕ скорости ходьбы игрока (~600), чтобы
	// бандит реально догонял шагающего игрока, но НИЖЕ спринта игрока (~1200) — от спринта можно
	// оторваться ценой расхода голода/жажды (бой остаётся проходимым). Применяется детерминированно
	// в BeginPlay (после Super), чтобы не зависеть от дефолта CMC/возможного оверрайда в BP.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Movement", meta = (DisplayPriority = "2"))
	float BanditWalkSpeed = 650.0f;

	// Через сколько секунд после смерти Destroy тела (даём отыграть рэгдолл).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Death", meta = (DisplayPriority = "3"))
	float CorpseLifeSpan = 5.0f;

	// --- Лут при смерти (GDD §7.8: «враги дают деньги, изношенное оружие») ---
	// Деньги: случайно в [LootMoneyMin..Max]. Бандит DRAFT 10-30 (GDD §7.6 экономика).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (DisplayPriority = "4"))
	float LootMoneyMin = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	float LootMoneyMax = 30.0f;

	// Шанс, что вместе с деньгами упадут расходники из LootTable. Ринат: 35% НЕ менять.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0.0", ClampMax = "1.0", DisplayPriority = "5"))
	float LootItemDropChance = 0.35f;

	// Сколько расходников падает при удачном броске: случайно в [Min..Max]. Ринат 07-17:
	// «пока что-то одно из этого или два» — дефолт 1..2. Max меньше Min трактуется как Min.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "1", DisplayPriority = "6"))
	int32 LootItemCountMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (ClampMin = "1", DisplayPriority = "7"))
	int32 LootItemCountMax = 2;

	// Таблица возможных расходников: каждая выпавшая единица выбирается отсюда РАВНОВЕРОЯТНО
	// (повторы допустимы). Дефолт из конструктора: Консервы / Вода / Бинт. Пустая таблица =
	// предметы не падают (только деньги).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot", meta = (DisplayPriority = "8"))
	TArray<FBanditLootEntry> LootTable;

	// Класс пикапа-лута (по умолчанию APickup, без BP/редактора).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TSubclassOf<APickup> PickupClass;

	// Спавнит лут (деньги + шанс 1-2 расходников из LootTable) в позиции трупа.
	// Вызывается из HandleDeath.
	void DropLoot();

	// Реакция на смерть из делегата UStatsComponent::OnDeath.
	// Переопределяет базовую заглушку: у врага смерть идёт через UStatsComponent,
	// поэтому здесь — полноценная обработка (рэгдолл, отключение ИИ, Destroy с задержкой).
	// UFUNCTION-спецификатор НЕ повторяем (наследуется от базовой virtual UFUNCTION) —
	// этого достаточно для AddDynamic к делегату OnDeath.
	virtual void HandleDeath() override;

public:
	// Перехватываем стандартный пайплайн урона UE и роутим в UStatsComponent,
	// чтобы существующая система оружия (Weapon->TakeDamage) работала без правок.
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Stats")
	UStatsComponent* GetStats() const { return Stats; }
};
