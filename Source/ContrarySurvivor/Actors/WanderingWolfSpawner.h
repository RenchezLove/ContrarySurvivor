// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "UObject/WeakObjectPtr.h"
#include "WanderingWolfSpawner.generated.h"

class ACharacter;
class APawn;
class USceneComponent;
class USphereComponent;

/**
 * Периодический спавн «бродячего» волка возле игрока (задача Рината, дословно):
 * «Думаю что бы мир не казался пустым - можно переодически (если игрок не в зеленой зоне
 * и не возле одной из баз противника) спавнить волка не подалеку.»
 *
 * ИНСТРУМЕНТ-актор (образец оформления — AMasterEnemyBase): команда делает класс, на уровень
 * его ставит Ринат сам; все тюнинг-параметры EditAnywhere наверху Details (meta
 * DisplayPriority). Достаточно ОДНОГО экземпляра на уровне: позиция актора на логику НЕ
 * влияет — спавн идёт вокруг ИГРОКА. Класс волка назначается в BP-обёртке
 * (WolfClass = BP_Wolf); путь к BP в C++ не хардкодим (как EnemyClass у AMasterEnemyBase).
 *
 * ЛОГИКА (повторяющийся таймер SpawnInterval, за тик — максимум один волк):
 *  - НЕ спавним, если: выключен (bEnabled); лимит живых бродячих волков исчерпан
 *    (MaxAliveWolves — мёртвые/уничтоженные из лимита выбывают); игрок в «зелёной зоне»
 *    костра (пересечение с триггером ACampfire) или в деревне (AVillageZone, ADR-036
 *    «деревня = безопасная зона», отключаемо bBlockWhenPlayerInVillage); игрок ближе
 *    EnemyBaseExclusionRadius к любой базе врагов (AMasterEnemyBase — логово волков /
 *    база бандитов);
 *  - точка спавна: случайное направление и дистанция MinSpawnDistance..MaxSpawnDistance от
 *    игрока; ОБЯЗАТЕЛЬНА навмеш-проекция (не село на навмеш — попытка тихо пропускается),
 *    высота — трасса до пола (SpawnPlacement, как у AMasterEnemyBase); точка не в деревне,
 *    не ближе CampfireExclusionRadius к костру и EnemyBaseExclusionRadius к базе врагов;
 *  - точка НЕ в кадре камеры игрока (экранная проекция, тот же приём, что честность
 *    бандита-стрелка ADR-035) — волк не появляется на глазах. MinSpawnDistance держать
 *    больше видимой дистанции камеры, экранная проверка — страховка;
 *  - до MaxSpawnAttempts случайных точек за тик; все мимо — молча ждём следующего тика.
 *
 * Поводок бродячему волку НЕ назначаем: у врага без базы AEnemyAIController::OnPossess сам
 * ставит дом = точка спавна, радиус = DefaultLeashRadius (см. EnemyAIController.h) — волк
 * гоняется в разумном радиусе от места появления и возвращается.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AWanderingWolfSpawner : public AActor
{
	GENERATED_BODY()

public:
	AWanderingWolfSpawner();

protected:
	virtual void BeginPlay() override;

	// === НАСТРОЙКИ (наверху Details, тюнинг Рината на размещённом экземпляре) ===

	// Главный выключатель периодического спавна (можно переключать и в рантайме из BP).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner", meta = (DisplayPriority = "1"))
	bool bEnabled = true;

	// Класс волка (BP_Wolf назначает оператор в BP-обёртке; в C++ пусто — путь не хардкодим).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner", meta = (DisplayPriority = "2"))
	TSubclassOf<ACharacter> WolfClass;

	// Период попыток спавна, сек.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner", meta = (ClampMin = "1.0", UIMin = "1.0", DisplayPriority = "3"))
	float SpawnInterval = 30.0f;

	// Минимальная дистанция точки спавна от игрока, см. Держать БОЛЬШЕ видимой дистанции
	// топ-даун камеры, чтобы волк не появлялся в кадре (экранная проверка — страховка).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner", meta = (ClampMin = "100.0", DisplayPriority = "4"))
	float MinSpawnDistance = 2500.0f;

	// Максимальная дистанция точки спавна от игрока, см.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner", meta = (ClampMin = "100.0", DisplayPriority = "5"))
	float MaxSpawnDistance = 4000.0f;

	// Лимит ОДНОВРЕМЕННО живых бродячих волков от этого спавнера. Мёртвые (UStatsComponent
	// IsDead) и уничтоженные из лимита выбывают — на их место придут новые.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner", meta = (ClampMin = "1", UIMin = "1", DisplayPriority = "6"))
	int32 MaxAliveWolves = 3;

	// Радиус исключения вокруг баз врагов (AMasterEnemyBase: логово волков / база бандитов),
	// см: игрок ближе — спавна нет; и сама точка спавна ближе к базе не берётся.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner", meta = (ClampMin = "0.0", DisplayPriority = "7"))
	float EnemyBaseExclusionRadius = 5000.0f;

	// Точка спавна не ближе этого радиуса (см) к любому костру — чтобы волк не возник прямо
	// у безопасной зоны. «Игрок в зоне костра» проверяется отдельно, триггером самого костра.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner", meta = (ClampMin = "0.0", DisplayPriority = "8"))
	float CampfireExclusionRadius = 1000.0f;

	// Не спавнить, пока игрок в деревне (ADR-036 «деревня = безопасная зона»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner", meta = (DisplayPriority = "9"))
	bool bBlockWhenPlayerInVillage = true;

	// Сколько случайных точек перебрать за один тик, прежде чем тихо сдаться до следующего.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner|Advanced", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxSpawnAttempts = 8;

	// Запас к границе экрана (пиксели) при проверке «точка в кадре»: точка чуть ЗА краем
	// кадра всё ещё считается видимой — чтобы волк не «проявлялся» по самой кромке экрана.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WolfSpawner|Advanced", meta = (ClampMin = "0.0"))
	float ScreenEdgeMargin = 100.0f;

	// === Компоненты ===

	// Корень-трансформ (placeable; позиция актора на логику спавна не влияет).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WolfSpawner")
	USceneComponent* SceneRoot;

	// Маленькая каркас-сфера — только чтобы актор был виден и кликабелен во вьюпорте
	// (в игре скрыта, коллизии и навигации нет). Радиус фиксированный, смысла не несёт.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WolfSpawner")
	USphereComponent* EditorMarker;

private:
	FTimerHandle SpawnTimerHandle;

	// Живые бродячие волки (weak-ссылки: уничтоженные отваливаются сами, мёртвых чистим сами).
	TArray<TWeakObjectPtr<ACharacter>> AliveWolves;

	// Тик таймера: проверки зон/лимита, подбор валидной точки, спавн одного волка.
	void TrySpawnWolf();

	// Выкидывает из списка уничтоженных и мёртвых (UStatsComponent::IsDead) волков.
	void PruneDeadWolves();

	// Игрок в безопасной зоне: внутри триггера любого костра ИЛИ (опционально) в деревне.
	bool IsPlayerInSafeArea(const APawn& Player) const;

	// Игрок ближе EnemyBaseExclusionRadius (XY) к любой базе врагов?
	bool IsPlayerNearEnemyBase(const APawn& Player) const;

	// Точка годна по зонам: не в деревне, не у костра, не у базы врагов.
	bool IsSpawnPointClearOfZones(const FVector& Point) const;

	// Точка попадает в кадр камеры игрока (с запасом ScreenEdgeMargin)? Приём ADR-035.
	bool IsPointOnPlayerScreen(const FVector& Point) const;
};
