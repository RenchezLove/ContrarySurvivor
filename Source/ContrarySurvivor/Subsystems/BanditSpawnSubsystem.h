// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BanditSpawnSubsystem.generated.h"

class AEnemyCharacter;
class AMasterInventoryItem;
class APickup;

/**
 * Зональный спавнер бандитов (Фаза 5, квест «зачистить базу») — зеркало UWolfSpawnSubsystem.
 *
 * ДИЗАЙН: деревня — мирная зона. База бандитов находится ЮЖНЕЕ деревни (у сарая). Когда игрок
 * ВПЕРВЫЕ подходит к базе (XY-дистанция ≤ ActivationRadius), там одноразово спавнится группа
 * бандитов — место выполнения квеста «зачистить базу».
 *
 * Реализация: UWorldSubsystem (создаётся автоматически для каждого игрового мира). На
 * OnWorldBeginPlay запускает повторяющийся таймер (~0.5с), который проверяет горизонтальную
 * дистанцию игрока до BanditBaseLocation. При первом входе в радиус спавнит NumBandits
 * бандитов вокруг базы (высота каждого — трасса до пола через SpawnPlacement, НЕ от игрока) и
 * одноразово выставляет bBanditsSpawned=true (таймер после этого гасится).
 *
 * ВАЖНО: спавним BP_EnemyBandit (модульный визуал/меши назначены в редакторе), а не голый
 * C++ AEnemyCharacter. Класс грузится в конструкторе через FClassFinder; fallback —
 * AEnemyCharacter::StaticClass(). AI/бой бандита уже работает у placed-актора и здесь не трогаются.
 */
UCLASS()
class CONTRARYSURVIVOR_API UBanditSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UBanditSpawnSubsystem();

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// Центр базы бандитов (для QA-телепорта игрока к базе, клавиша Z — зеркало V/Логово). Z тут
	// условный — точную высоту берёт трасса до пола; XY = место выполнения квеста «зачистить базу».
	// Источник истины тот же BanditBaseLocation, по которому активируется спавн бандитов «по приближению».
	FVector GetBanditBaseLocation() const { return BanditBaseLocation; }

protected:
	// Класс бандита для спавна. Дефолт грузится из BP_EnemyBandit (модульные меши назначены в
	// редакторе). EditAnywhere — чтобы подменить класс без перекомпиляции.
	UPROPERTY(EditAnywhere, Category = "BanditSpawn")
	TSubclassOf<AEnemyCharacter> BanditClass;

	// Центр базы бандитов — ЮЖНЕЕ деревни (−Y, у сарая). BugReport 12: база отодвинута ПОДАЛЬШЕ
	// от деревни → дефолт (0,−3500,Z), ≥2500 от края деревни, симметрично Логову волков (0,+3500).
	// Точную высоту КАЖДОГО бандита берём трассой до пола, поэтому дефолтный Z тут не критичен.
	// EditAnywhere — чтобы unreal-operator подвинул базу под реальную карту без перекомпиляции
	// (координаты согласованы с оператором).
	UPROPERTY(EditAnywhere, Category = "BanditSpawn")
	FVector BanditBaseLocation = FVector(0.0f, -3500.0f, 0.0f);

	// Радиус активации (см): когда игрок ВПЕРВЫЕ входит в этот радиус (по XY) от базы — спавним
	// бандитов. ~1200 см ≈ 12 м.
	UPROPERTY(EditAnywhere, Category = "BanditSpawn")
	float ActivationRadius = 1200.0f;

	// Сколько бандитов спавнить (квест «зачистить базу»).
	UPROPERTY(EditAnywhere, Category = "BanditSpawn")
	int32 NumBandits = 3;

	// Разброс бандитов по кругу вокруг центра базы (см).
	UPROPERTY(EditAnywhere, Category = "BanditSpawn")
	float SpreadRadius = 300.0f;

	// Одноразовый флаг: бандиты на базе уже заспавнены. EditAnywhere для дебага/тюнинга.
	UPROPERTY(EditAnywhere, Category = "BanditSpawn")
	bool bBanditsSpawned = false;

	// --- Квест-предмет «Ноутбук» (Фаза 5, кв.2 старосты) ---
	// Класс квест-предмета, который кладётся в пикап на базе (по умолчанию AQuestItem, категория
	// Quest — не теряется при смерти, не используется). Подбирается игроком по E (как любой пикап).
	UPROPERTY(EditAnywhere, Category = "BanditSpawn|Quest")
	TSubclassOf<AMasterInventoryItem> QuestItemClass;

	// Класс пикапа-носителя (по умолчанию APickup, без BP/редактора).
	UPROPERTY(EditAnywhere, Category = "BanditSpawn|Quest")
	TSubclassOf<APickup> PickupClass;

	// Имя квест-предмета (выставляется заспавненному ItemName -> должно совпадать с RequiredItemName
	// кв.2 у старосты: «Ноутбук»).
	UPROPERTY(EditAnywhere, Category = "BanditSpawn|Quest")
	FString QuestItemName = TEXT("Ноутбук");

	// Период проверки дистанции игрока до базы (сек).
	float ActivationCheckPeriod = 0.5f;

	// Задержка (сек) между входом игрока в радиус и фактическим спавном бандитов. Нужна, чтобы
	// Navigation Invoker на игроке успел построить навмеш-тайлы вокруг базы (игрок только что
	// телепортнулся) → бандиты садятся navmesh=yes, а не floor-trace. EditAnywhere — на тюнинг.
	UPROPERTY(EditAnywhere, Category = "BanditSpawn")
	float SpawnDelay = 1.2f;

private:
	FTimerHandle ActivationTimerHandle;

	// Таймер отложенного спавна (после паузы на построение навмеша).
	FTimerHandle SpawnDelayTimerHandle;

	// Тик проверки активации (по таймеру): сравнивает XY-дистанцию игрока до базы.
	void CheckActivation();

	// Отложенный спавн бандитов + ноутбука (вызывается по SpawnDelayTimerHandle).
	void DoDelayedSpawn();

	// Спавнит NumBandits бандитов вокруг BanditBaseLocation (высота каждого — трасса до пола).
	void SpawnBanditsAtBase();

	// Спавнит пикап с квест-предметом «Ноутбук» в центре базы (высота — трасса до пола).
	void SpawnQuestNotebookAtBase();
};
