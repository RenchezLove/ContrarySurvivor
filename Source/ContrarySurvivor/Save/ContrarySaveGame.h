// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ContrarySaveGame.generated.h"

/**
 * Сейв игрока (GDD §7.8). Хранит статы выживания + позицию игрока (точка респауна).
 * Запись/чтение через UGameplayStatics::SaveGameToSlot / LoadGameFromSlot.
 * Костёр (ACampfire) делает автосейв при входе в безопасную зону.
 */
UCLASS()
class CONTRARYSURVIVOR_API UContrarySaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// Есть ли валидные данные (false у свежесозданного объекта без записи).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save")
	bool bHasData = false;

	// --- Статы (UStatsComponent) ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Stats")
	float Health = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Stats")
	float Hunger = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Stats")
	float Thirst = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Stats")
	float Money = 50.0f;

	// --- Позиция/точка респауна ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Transform")
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Transform")
	FRotator PlayerRotation = FRotator::ZeroRotator;

	// --- Задел: инвентарь (GDD §7.8) ---
	// Пути классов предметов рюкзака. ЗАДЕЛ: полноценная сериализация инвентаря —
	// после доработки UInventoryComponent (см. эскалацию по категориям предметов).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Inventory")
	TArray<FString> InventoryItemClassPaths;

	// --- Задел: экипированная броня по слотам (Фаза 4) ---
	// Пути классов экипированной брони (Head/Torso/Legs); пусто = слот свободен.
	// ЗАДЕЛ: запись выполняется в APlayerCharacter::SaveGame. Авто-восстановление при
	// загрузке — будущая волна (сейчас игрок экипирует дефолтную броню в BeginPlay).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Equipment")
	FString EquippedHeadArmorClassPath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Equipment")
	FString EquippedTorsoArmorClassPath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Save|Equipment")
	FString EquippedLegsArmorClassPath;
};
