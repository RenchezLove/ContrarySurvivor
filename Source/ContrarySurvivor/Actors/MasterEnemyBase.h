// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "MasterEnemyBase.generated.h"

class ACharacter;
class AMasterInventoryItem;
class APickup;
class USceneComponent;

/**
 * Размещаемая зона спавна врагов «по приближению» (A5: замена UWolfSpawnSubsystem +
 * UBanditSpawnSubsystem на один параметризуемый АКТОР, ADR-024 «BP-наследник C++»).
 *
 * ДИЗАЙН (как у прежних сабсистем, поведение НЕ меняем): деревня — мирная зона, враги ждут
 * в своей зоне (логово/база) и спавнятся ОДНОРАЗОВО, когда игрок ВПЕРВЫЕ подходит на
 * ActivationRadius (горизонтальная XY-дистанция до АКТОРА). Центр зоны = GetActorLocation()
 * размещённого актора (хардкод координат ±3500 убран — место задаёт оператор расстановкой).
 *
 * Активация: повторяющийся таймер (ActivationCheckPeriod) проверяет дистанцию игрока; при
 * входе в радиус — пауза SpawnDelay (Nav Invoker достраивает тайлы), затем спавн NumToSpawn
 * врагов по кругу (SpreadRadius), навмеш-проекция + floor-trace Z (SpawnPlacement). Опц.
 * квест-предмет (ноутбук кв.2) кладётся пикапом в центре.
 *
 * BP-наследники (editor): BP_WolfDen (EnemyClass=BP_Wolf, NumToSpawn=4, север),
 * BP_BanditBase (EnemyClass=BP_EnemyBandit, NumToSpawn=3, bSpawnQuestItem, юг). Числа/классы
 * задаёт BP — в C++ нейтральные дефолты (NumToSpawn=1, EnemyClass пуст).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AMasterEnemyBase : public AActor
{
	GENERATED_BODY()

public:
	AMasterEnemyBase();

protected:
	virtual void BeginPlay() override;

	// Корень-трансформ (placeable). Меш/иконку задаёт BP при желании.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	USceneComponent* SceneRoot;

	// Класс врага для спавна (BP_Wolf / BP_EnemyBandit назначает оператор в BP-наследнике).
	// Нейтральный дефолт — пусто (без BP спавна не будет; число/класс не хардкодим в C++).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	TSubclassOf<ACharacter> EnemyClass;

	// Сколько врагов спавнить. Нейтральный дефолт 1; 4 (волки) / 3 (бандиты) ставит BP.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	int32 NumToSpawn = 1;

	// Радиус активации (см): первый вход игрока (XY) в радиус от актора → спавн. ~1200 ≈ 12 м.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	float ActivationRadius = 1200.0f;

	// Разброс врагов по кругу вокруг центра зоны (см).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	float SpreadRadius = 400.0f;

	// Задержка (сек) между входом игрока в радиус и фактическим спавном — Nav Invoker на игроке
	// успевает достроить навмеш-тайлы вокруг зоны (бандиты/волки садятся navmesh=yes). Тюнинг в BP.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	float SpawnDelay = 0.5f;

	// Период проверки дистанции игрока до зоны (сек).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	float ActivationCheckPeriod = 0.5f;

	// --- Опц. квест-предмет (напр. «Ноутбук» на базе бандитов, кв.2 старосты) ---

	// Класть ли квест-предмет в центре зоны при активации (BP_BanditBase ставит true).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnemyBase|Quest")
	bool bSpawnQuestItem = false;

	// Класс квест-предмета (дефолт AQuestItem — категория Quest, не теряется при смерти).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnemyBase|Quest")
	TSubclassOf<AMasterInventoryItem> QuestItemClass;

	// Класс пикапа-носителя (дефолт APickup).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnemyBase|Quest")
	TSubclassOf<APickup> PickupClass;

	// Имя квест-предмета (должно совпадать с RequiredItemName квеста старосты, напр. «Ноутбук»).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnemyBase|Quest")
	FString QuestItemName = TEXT("Ноутбук");

private:
	FTimerHandle ActivationTimerHandle;
	FTimerHandle SpawnDelayTimerHandle;

	// Одноразовый флаг: зона уже активирована (спавн произошёл/запланирован).
	bool bActivated = false;

	// Тик проверки активации (по таймеру): XY-дистанция игрока до центра зоны.
	void CheckActivation();

	// Отложенный спавн (после паузы на построение навмеша): враги + опц. квест-предмет.
	void DoSpawn();

	// Спавнит NumToSpawn врагов по кругу вокруг центра зоны (высота — трасса до пола).
	void SpawnEnemies();

	// Спавнит пикап с квест-предметом в центре зоны (если bSpawnQuestItem).
	void SpawnQuestItem();
};
