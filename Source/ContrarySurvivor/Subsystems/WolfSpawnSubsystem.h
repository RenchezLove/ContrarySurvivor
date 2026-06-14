// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WolfSpawnSubsystem.generated.h"

class AWolfCharacter;

/**
 * Зональный спавнер волков (Фаза 3/5, GDD §7.1) — БЕЗ расстановки в редакторе.
 *
 * ДИЗАЙН (правка Рината, 2026-06-14): деревня — мирная «зелёная зона», волки в ней НЕ
 * спавнятся. Волки живут в «Логове волков» (Wolf Den) ЗА деревней и появляются по
 * ПРИБЛИЖЕНИЮ игрока к Логову — это место выполнения kill-квеста: игрок выходит из
 * деревни → доходит до Логова → там бой.
 *
 * Реализация: UWorldSubsystem (создаётся автоматически для каждого игрового мира,
 * не требует актора-спавнера/reparent BP GameMode). На OnWorldBeginPlay запускает
 * повторяющийся таймер (~0.5с), который проверяет горизонтальную дистанцию игрока до
 * WolfDenLocation. При ПЕРВОМ входе в ActivationRadius спавнит NumWolves волков вокруг
 * Логова (высота каждого — трасса до пола через SpawnPlacement, НЕ наследуется от игрока)
 * и одноразово выставляет bWolvesSpawned=true (таймер после этого гасится).
 *
 * ЗАДЕЛ: логика «зональный спавн по приближению» здесь зашита под волков. Если позже
 * понадобится база бандитов — вынести WolfDenLocation/ActivationRadius/NumWolves в общий
 * параметризуемый зональный спавнер. Сейчас сознательно НЕ обобщаю (KISS).
 */
UCLASS()
class CONTRARYSURVIVOR_API UWolfSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// Центр «Логова волков» (для QA-телепорта игрока к Логову, клавиша V). Z тут условный —
	// точную высоту берёт трасса до пола; XY = место выполнения kill-квеста.
	FVector GetWolfDenLocation() const { return WolfDenLocation; }

protected:
	// Класс волка для спавна (по умолчанию AWolfCharacter — без BP/редактора).
	UPROPERTY()
	TSubclassOf<AWolfCharacter> WolfClass;

	// Центр «Логова волков» (Wolf Den) — опушка леса СЕВЕРНЕЕ деревни (+Y; база бандитов
	// будет южнее). Дефолт (0,2200,Z): точную высоту КАЖДОГО волка берём трассой до пола,
	// поэтому дефолтный Z тут не критичен. EditAnywhere — чтобы unreal-operator подвинул
	// Логово под реальную карту без перекомпиляции.
	UPROPERTY(EditAnywhere, Category = "WolfSpawn")
	FVector WolfDenLocation = FVector(0.0f, 2200.0f, 0.0f);

	// Радиус активации (см): когда игрок ВПЕРВЫЕ входит в этот радиус (по XY) от Логова —
	// спавним волков. ~1200 см ≈ 12 м: игрок уже вышел из деревни и подошёл к лесу.
	UPROPERTY(EditAnywhere, Category = "WolfSpawn")
	float ActivationRadius = 1200.0f;

	// Сколько волков спавнить. 5 — под Kill-квест «убить 5 волков» (Фаза 5).
	UPROPERTY(EditAnywhere, Category = "WolfSpawn")
	int32 NumWolves = 5;

	// Одноразовый флаг: волки в Логове уже заспавнены. EditAnywhere для дебага/тюнинга.
	UPROPERTY(EditAnywhere, Category = "WolfSpawn")
	bool bWolvesSpawned = false;

	// Разброс волков по кругу вокруг центра Логова (см).
	UPROPERTY(EditAnywhere, Category = "WolfSpawn")
	float DenSpreadRadius = 400.0f;

	// Период проверки дистанции игрока до Логова (сек).
	float ActivationCheckPeriod = 0.5f;

private:
	FTimerHandle ActivationTimerHandle;

	// Тик проверки активации (по таймеру): сравнивает XY-дистанцию игрока до Логова.
	void CheckActivation();

	// Спавнит NumWolves волков вокруг WolfDenLocation (высота каждого — трасса до пола).
	void SpawnWolvesAtDen();
};
