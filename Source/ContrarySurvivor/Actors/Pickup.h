// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "Pickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class AMasterInventoryItem;

/**
 * Подбираемый лут (Фаза 4, экономика — GDD §7.8: «враги дают деньги/изношенное оружие»).
 *
 * Один актор-пикап может нести ДЕНЬГИ (MoneyAmount) и/или ПРЕДМЕТ (CarriedItem). Подбор —
 * по overlap пешки игрока (APlayerCharacter): деньги уходят в UStatsComponent, предмет — в
 * рюкзак (UInventoryComponent), затем пикап уничтожается. Editor-независимо: не Pawn и не
 * несёт UStatsComponent, поэтому НИКОГДА не попадает под авто-лок/таргетинг игрока.
 *
 * Дроп с врага: статический хелпер DropLoot (вызывается из HandleDeath бандита/волка).
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API APickup : public AActor
{
	GENERATED_BODY()

public:
	APickup();

	// Инициализирует лут пикапа (вызывается сразу после спавна). Money — сумма денег,
	// CarriedItem — предмет (уже заспавненный, скрытый, без коллизии) либо nullptr.
	void InitLoot(float Money, AMasterInventoryItem* InCarriedItem);

	// Создаёт лут на земле: при необходимости спавнит предмет (по ItemDropChance) и пикап,
	// который несёт MoneyAmount + предмет. Удобный путь для дропа с врага одной строкой.
	// Возвращает заспавненный пикап (или nullptr).
	static APickup* DropLoot(UWorld* World, const FVector& Location, float MoneyAmount,
		TSubclassOf<AMasterInventoryItem> ItemClass, float ItemDropChance,
		TSubclassOf<APickup> PickupClass);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Триггер подбора: overlap по Pawn (как у костра-сейва).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	USphereComponent* PickupTrigger;

	// Визуальный плейсхолдер (без коллизии). Реальный меш/иконку задаёт BP/operator.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	UStaticMeshComponent* MeshComponent;

	// Радиус подбора (см). DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	float PickupRadius = 90.0f;

	// Сумма денег в пикапе (0 = нет денег).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	float MoneyAmount = 0.0f;

	// Предмет, который пикап отдаёт в рюкзак при подборе (nullptr = только деньги).
	UPROPERTY()
	AMasterInventoryItem* CarriedItem = nullptr;

	UFUNCTION()
	void OnPickupBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	// true, если лут уже подобран игроком (чтобы EndPlay не уничтожил отданный предмет).
	bool bCollected = false;
};
