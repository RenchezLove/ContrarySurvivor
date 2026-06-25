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
class UEnemySpawnPointComponent;

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
 * входе в радиус — пауза SpawnDelay (Nav Invoker достраивает тайлы), затем спавн врагов,
 * навмеш-проекция + floor-trace Z (SpawnPlacement). Опц. квест-предмет (ноутбук кв.2) кладётся
 * пикапом в центре.
 *
 * ТОЧКИ СПАВНА (задача Рината): позиции врагов берутся из компонентов-маркеров
 * UEnemySpawnPointComponent, которые дизайнер расставляет/двигает во вьюпорте BP. Сколько
 * точек размещено — столько врагов и спавнится (в позициях точек, лицом по стрелке точки).
 * Если в BP не размещено ни одной точки — fallback на старое поведение: NumToSpawn врагов по
 * кругу (SpreadRadius) вокруг центра актора. C++ создаёт одну дефолтную точку, чтобы базовый
 * актор уже имел видимый перемещаемый маркер.
 *
 * BP-наследники (editor): BP_WolfDen (EnemyClass=BP_Wolf, 4 точки, север),
 * BP_BanditBase (EnemyClass=BP_EnemyBandit, 3 точки, bSpawnQuestItem, юг). Классы и точки
 * задаёт BP — в C++ нейтральные дефолты (EnemyClass пуст, одна точка-образец).
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

	// Дефолтная точка спавна — образец, чтобы у базового актора уже был видимый перемещаемый
	// маркер. Дизайнер двигает её и/или добавляет ещё точек (Enemy Spawn Point) в дереве BP.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "EnemyBase")
	UEnemySpawnPointComponent* DefaultSpawnPoint;

	// Класс врага для спавна (BP_Wolf / BP_EnemyBandit назначает оператор в BP-наследнике).
	// Нейтральный дефолт — пусто (без BP спавна не будет; класс не хардкодим в C++).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase")
	TSubclassOf<ACharacter> EnemyClass;

	// Дистанция спавна (см): первый вход игрока (XY) в этот радиус от актора → спавн. Чем больше,
	// тем дальше игрок, когда враги появляются (чтобы не видеть спавн вблизи). Тюнинг per-instance
	// в BP. 3500 ≈ 35 м (спавн вне зоны видимости, до подхода игрока к базе).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase", meta = (ClampMin = "100.0", UIMin = "100.0"))
	float ActivationRadius = 3500.0f;

	// FALLBACK (только если в BP НЕ размещено ни одной точки спавна): сколько врагов спавнить по
	// кругу вокруг центра. Если точки расставлены — число врагов = число точек, это поле не используется.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Fallback")
	int32 NumToSpawn = 1;

	// FALLBACK: радиус круговой раскладки врагов вокруг центра (см). Используется только без точек спавна.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Fallback")
	float SpreadRadius = 400.0f;

	// Задержка (сек) между входом игрока в радиус и фактическим спавном — Nav Invoker на игроке
	// успевает достроить навмеш-тайлы вокруг зоны (бандиты/волки садятся navmesh=yes). Тюнинг в BP.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase")
	float SpawnDelay = 0.5f;

	// Период проверки дистанции игрока до зоны (сек).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase")
	float ActivationCheckPeriod = 0.5f;

	// --- Опц. квест-предмет (напр. «Ноутбук» на базе бандитов, кв.2 старосты) ---

	// Класть ли квест-предмет в центре зоны при активации (BP_BanditBase ставит true).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Quest")
	bool bSpawnQuestItem = false;

	// Класс квест-предмета (дефолт AQuestItem — категория Quest, не теряется при смерти).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Quest")
	TSubclassOf<AMasterInventoryItem> QuestItemClass;

	// Класс пикапа-носителя (дефолт APickup).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Quest")
	TSubclassOf<APickup> PickupClass;

	// Имя квест-предмета (должно совпадать с RequiredItemName квеста старосты, напр. «Ноутбук»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyBase|Quest")
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

	// Спавнит врагов: по одному в позиции каждой размещённой точки спавна
	// (UEnemySpawnPointComponent); если точек нет — fallback на круговую раскладку NumToSpawn.
	void SpawnEnemies();

	// Собирает мировые трансформы всех размещённых точек спавна (UEnemySpawnPointComponent).
	// Пусто, если дизайнер не оставил ни одной точки в BP (тогда работает fallback).
	void CollectSpawnPointTransforms(TArray<FTransform>& OutTransforms) const;

	// Спавнит одного врага в позиции SpawnTransform (XY на навмеш + floor-trace Z, Yaw из точки).
	void SpawnOneEnemy(const FTransform& SpawnTransform);

	// Спавнит пикап с квест-предметом в центре зоны (если bSpawnQuestItem).
	void SpawnQuestItem();
};
