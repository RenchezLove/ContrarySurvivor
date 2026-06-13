// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WolfSpawnSubsystem.generated.h"

class AWolfCharacter;

/**
 * Кодовый спавнер волков (Фаза 3, GDD §7.1) — БЕЗ расстановки в редакторе.
 *
 * Реализован как UWorldSubsystem: создаётся автоматически для каждого игрового мира
 * (PIE/Standalone), не требует размещения актора-спавнера или reparent BP GameMode.
 * На OnWorldBeginPlay (только в игровом мире) с небольшой задержкой (чтобы игрок успел
 * заспавниться) ставит 1-2 волков рядом с игроком, проецируя точки на навмеш.
 *
 * ДОПУЩЕНИЕ: «арена боя» в коде неизвестна (карта правится в редакторе), поэтому опорная
 * точка спавна — позиция игрока + смещение, спроецированное на навмеш. Точные координаты
 * арены может задать unreal-operator позже (или переопределить класс/число волков).
 */
UCLASS()
class CONTRARYSURVIVOR_API UWolfSpawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
	// Класс волка для спавна (по умолчанию AWolfCharacter — без BP/редактора).
	UPROPERTY()
	TSubclassOf<AWolfCharacter> WolfClass;

	// Сколько волков спавнить (draft: 1-2).
	int32 NumWolves = 2;

	// Задержка перед спавном (сек): даём игроку/контроллеру заспавниться.
	float SpawnDelay = 1.5f;

	// Дистанция спавна от игрока (см) и разброс.
	float SpawnDistance = 800.0f;

private:
	FTimerHandle SpawnTimerHandle;

	// Спавнит волков рядом с игроком (вызывается по таймеру).
	void SpawnWolves();
};
