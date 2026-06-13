// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "TraderSpawnSubsystem.generated.h"

class ATraderNPC;

/**
 * Кодовый спавнер торговца (Фаза 4, экономика — GDD §7.1: «в MVP 1 торговец»), БЕЗ
 * расстановки в редакторе. По образцу UWolfSpawnSubsystem: UWorldSubsystem, авто для
 * каждого игрового мира. На OnWorldBeginPlay (с задержкой, чтобы игрок успел заспавниться)
 * ставит 1 торговца рядом с игроком, проецируя точку на навмеш.
 *
 * ДОПУЩЕНИЕ: точная позиция «деревни» в коде неизвестна (карта правится в редакторе),
 * поэтому опорная точка — игрок + смещение, спроецированное на навмеш. Точные координаты
 * (и фикс-позицию в деревне) задаст unreal-operator позже либо переопределит здесь.
 */
UCLASS()
class CONTRARYSURVIVOR_API UTraderSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	// Класс торговца (по умолчанию ATraderNPC — без BP/редактора).
	UPROPERTY()
	TSubclassOf<ATraderNPC> TraderClass;

	// Задержка перед спавном (сек): даём игроку заспавниться.
	float SpawnDelay = 1.5f;

	// Дистанция спавна от игрока (см) — торговец стоит неподалёку, дойти пешком.
	float SpawnDistance = 600.0f;

private:
	FTimerHandle SpawnTimerHandle;

	void SpawnTrader();
};
