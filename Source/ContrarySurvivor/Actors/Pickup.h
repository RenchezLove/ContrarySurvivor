// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "Pickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class AMasterInventoryItem;
class APlayerCharacter;

/**
 * Подбираемый лут (Фаза 4, экономика — GDD §7.8: «враги дают деньги/изношенное оружие»).
 *
 * Один актор-пикап может нести ДЕНЬГИ (MoneyAmount) и/или ПРЕДМЕТ (CarriedItem). Подбор —
 * по КЛАВИШЕ E (контекстный interact, Фаза 4): контроллер находит ближайший пикап и зовёт
 * Collect() — деньги уходят в UStatsComponent, предмет — в рюкзак (UInventoryComponent), затем
 * пикап уничтожается. Авто-подбор по overlap УБРАН (был нестабилен — BUG2). Editor-независимо:
 * не Pawn и не несёт UStatsComponent, поэтому НИКОГДА не попадает под авто-лок/таргетинг игрока.
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

	// A4/ADR-027: «мешок» из НЕСКОЛЬКИХ предметов (дроп расходников при смерти). Предметы уже
	// сняты из рюкзака и скрыты/без коллизии. Collect отдаёт ВСЕ в рюкзак; EndPlay (если не
	// подобран) их уничтожает. Совместимо с одиночным CarriedItem (обрабатываются оба).
	void InitLootBag(const TArray<AMasterInventoryItem*>& Items);

	// Подбор по КЛАВИШЕ E (контекстный interact, Фаза 4 — решение Рината/game-lead): надёжно
	// начисляет деньги (UStatsComponent) и кладёт предмет в рюкзак (UInventoryComponent), затем
	// уничтожает пикап. Возвращает true ТОЛЬКО если весь имеющийся лут реально начислен — иначе
	// пикап остаётся на земле (повторная попытка), а не «исчезает без начисления» (фикс BUG2).
	bool Collect(APlayerCharacter* Player);

	// Есть ли в пикапе что подбирать (для контекстной подсказки на HUD).
	bool HasLoot() const;

	// Создаёт лут на земле: при необходимости спавнит предмет (по ItemDropChance) и пикап,
	// который несёт MoneyAmount + предмет. Удобный путь для дропа с врага одной строкой.
	// ItemDisplayName (опц.): если задано и предмет заспавнен — выставляет ему ItemName
	// (понятное имя в рюкзаке/UI, напр. «Шкура волка»). QA force-drop (FQADebug::bForceDrop)
	// поднимает фактический шанс выпадения предмета до 100%.
	// Возвращает заспавненный пикап (или nullptr).
	static APickup* DropLoot(UWorld* World, const FVector& Location, float MoneyAmount,
		TSubclassOf<AMasterInventoryItem> ItemClass, float ItemDropChance,
		TSubclassOf<APickup> PickupClass, const FString& ItemDisplayName = FString());

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Триггер подбора: overlap по Pawn (как у костра-сейва).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	USphereComponent* PickupTrigger;

	// Визуальный плейсхолдер (без коллизии). Реальный меш/иконку задаёт BP/operator.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pickup")
	UStaticMeshComponent* MeshComponent;

	// Сумма денег в пикапе (0 = нет денег). D8: EditAnywhere — задаётся на РАЗМЕЩЁННОМ
	// экземпляре (лут точек интереса); рантайм-дроп по-прежнему пишет её через InitLoot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0.0", DisplayPriority = "1"))
	float MoneyAmount = 0.0f;

	// --- D8: размещаемый лут (заполняет дизайнер на экземпляре на карте) ---
	// BeginPlay спавнит предметы скрытыми (как DropLoot) и заводит их в стандартный
	// механизм CarriedItems — подбор той же клавишей E, ничего нового в Collect.

	// Класс стартового предмета (nullptr = предмета нет). Особый случай: класс патронов
	// (AAmmoItem) спавнится ОДНОЙ пачкой со стаком PlacedItemCount, а не N пустыми копиями.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "2"))
	TSubclassOf<AMasterInventoryItem> PlacedItemClass;

	// Понятное имя предмета в рюкзаке/UI (пусто = имя класса по умолчанию).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "3"))
	FString PlacedItemDisplayName;

	// Сколько предметов положить (для AAmmoItem — размер стака одной пачки).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "1", DisplayPriority = "4"))
	int32 PlacedItemCount = 1;

	// Патроны В ДОПОЛНЕНИЕ к предмету (одна пачка AAmmoItem с этим стаком; 0 = без патронов).
	// Отдельное поле, потому что точка интереса несёт «расходник И патроны» одним пикапом,
	// а слот PlacedItemClass один (аналог поля денег).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0", DisplayPriority = "5"))
	int32 PlacedAmmoAmount = 0;

	// Предмет, который пикап отдаёт в рюкзак при подборе (nullptr = только деньги).
	UPROPERTY()
	AMasterInventoryItem* CarriedItem = nullptr;

	// A4/ADR-027: несколько предметов в «мешке» (дроп расходников при смерти). Отдаются все при
	// Collect; уничтожаются в EndPlay, если мешок не подобрали. UPROPERTY — держит их живыми (GC).
	UPROPERTY()
	TArray<AMasterInventoryItem*> CarriedItems;

private:
	// D8: спавнит размещённый лут (PlacedItemClass/PlacedAmmoAmount) скрытыми предметами
	// в CarriedItems. Зовётся из BeginPlay только в игровом мире.
	void SpawnPlacedLoot();

	// true, если лут уже подобран игроком (чтобы EndPlay не уничтожил отданный предмет).
	bool bCollected = false;
};
