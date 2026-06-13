// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "ElderSpawnSubsystem.generated.h"

class AElderNPC;

/**
 * Кодовый спавнер старосты (Фаза 5, квестодатель — GDD §7.7), БЕЗ расстановки в редакторе.
 * По образцу UTraderSpawnSubsystem/UWolfSpawnSubsystem: UWorldSubsystem, авто для каждого
 * игрового мира. На OnWorldBeginPlay (с задержкой) ставит 1 старосту на навмеше в деревне.
 *
 * Чтобы НЕ встать под торговцем (который спавнится тем же приёмом), берём бОльшую дистанцию
 * и начинаем перебор направлений с ПРОТИВОПОЛОЖНОЙ стороны (180°). Точную позицию в деревне
 * задаст unreal-operator позже либо переопределит здесь.
 */
UCLASS()
class CONTRARYSURVIVOR_API UElderSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	// Класс старосты (по умолчанию AElderNPC — без BP/редактора).
	UPROPERTY()
	TSubclassOf<AElderNPC> ElderClass;

	// Задержка перед спавном (сек): даём игроку заспавниться. Чуть позже торговца.
	float SpawnDelay = 1.7f;

	// Дистанция спавна от игрока (см). Больше, чем у торговца (600), чтобы не совпали.
	float SpawnDistance = 900.0f;

private:
	FTimerHandle SpawnTimerHandle;

	void SpawnElder();
};
