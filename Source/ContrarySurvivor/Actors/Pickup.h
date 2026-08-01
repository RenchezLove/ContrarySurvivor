// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "Pickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
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
	// Build 1.2: Money — деньги в мешке (доля потерянных при смерти монет, переопределение
	// Рината); подбираются штатным путём Collect, как MoneyAmount любого пикапа.
	void InitLootBag(const TArray<AMasterInventoryItem*>& Items, float Money = 0.0f);

	// Подбор по КЛАВИШЕ E (контекстный interact, Фаза 4 — решение Рината/game-lead): надёжно
	// начисляет деньги (UStatsComponent) и кладёт предмет в рюкзак (UInventoryComponent), затем
	// уничтожает пикап. Возвращает true ТОЛЬКО если весь имеющийся лут реально начислен — иначе
	// пикап остаётся на земле (повторная попытка), а не «исчезает без начисления» (фикс BUG2).
	bool Collect(APlayerCharacter* Player);

	// Есть ли в пикапе что подбирать (для контекстной подсказки на HUD).
	bool HasLoot() const;

	// Создаёт лут на земле: при необходимости спавнит предмет (по ItemDropChance) и пикап,
	// который несёт MoneyAmount + предмет. Удобный путь для дропа с врага одной строкой.
	// ItemDisplayName (опц.): служебный КЛЮЧ предмета (напр. «Шкура волка») — по нему
	// сходится зачёт квеста, НЕ переводится. ItemDisplayText (опц.): переводимое название
	// того же предмета для показа игроку; пусто — откат на ключ (ADR-050, порция 0).
	// QA force-drop (FQADebug::bForceDrop) поднимает фактический шанс выпадения до 100%.
	// Возвращает заспавненный пикап (или nullptr).
	static APickup* DropLoot(UWorld* World, const FVector& Location, float MoneyAmount,
		TSubclassOf<AMasterInventoryItem> ItemClass, float ItemDropChance,
		TSubclassOf<APickup> PickupClass, const FString& ItemDisplayName = FString(),
		const FText& ItemDisplayText = FText::GetEmpty());

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Build 1.2.1 (ТЗ А4): тик нужен ТОЛЬКО пульсации свечения — включается в BeginPlay
	// при включённом свечении с периодом > 0, иначе актор не тикает (как раньше).
	virtual void Tick(float DeltaTime) override;

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

	// СЛУЖЕБНЫЙ КЛЮЧ предмета (пусто = ключ класса по умолчанию). НЕ переводится: по нему
	// сходится зачёт квеста, если на уровень положен квест-предмет (ADR-050, порция 0).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "3"))
	FString PlacedItemDisplayName;

	// ПЕРЕВОДИМОЕ название этого же предмета, которое увидит игрок в рюкзаке (пусто =
	// название класса по умолчанию, а если и его нет — откат на ключ выше).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayPriority = "4"))
	FText PlacedItemDisplayText;

	// Сколько предметов положить (для AAmmoItem — размер стака одной пачки).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "1", DisplayPriority = "4"))
	int32 PlacedItemCount = 1;

	// Патроны В ДОПОЛНЕНИЕ к предмету (одна пачка AAmmoItem с этим стаком; 0 = без патронов).
	// Отдельное поле, потому что точка интереса несёт «расходник И патроны» одним пикапом,
	// а слот PlacedItemClass один (аналог поля денег).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0", DisplayPriority = "5"))
	int32 PlacedAmmoAmount = 0;

	// --- Build 1.2.1 (ТЗ А4, Ринат: «хочу добавить ему свечение») ---
	// Свечение мешка/свёртка: MID от материала меша (M_VColor несёт параметры
	// GlowColor/GlowIntensity с нулевыми дефолтами — остальные пользователи материала
	// не затронуты). Настройка на экземпляре/BP, дефолт ВЫКЛ (решение Рината).

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayName = "Свечение включено", DisplayPriority = "6"))
	bool bGlowEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (DisplayName = "Цвет свечения", DisplayPriority = "7", EditCondition = "bGlowEnabled"))
	FLinearColor GlowColor = FLinearColor(1.0f, 0.78f, 0.25f, 1.0f); // тёплый янтарный, в тон шнуру мешка

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0.0", DisplayName = "Сила свечения", DisplayPriority = "8", EditCondition = "bGlowEnabled"))
	float GlowStrength = 3.0f;

	// Период полного цикла пульсации, сек. 0 = ровное свечение без пульса (и без тика).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pickup", meta = (ClampMin = "0.0", DisplayName = "Период пульсации (сек, 0 = ровное)", DisplayPriority = "9", EditCondition = "bGlowEnabled"))
	float GlowPulsePeriod = 2.0f;

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

	// Build 1.2.1 (ТЗ А4): создаёт MID слота 0 и включает свечение (+тик при пульсации).
	void SetupGlow();

	// MID свечения (создан из материала меша). UPROPERTY — защита от GC.
	UPROPERTY()
	UMaterialInstanceDynamic* GlowMID = nullptr;

	// Накопленное время пульса (фаза синуса).
	float GlowTime = 0.0f;

	// true, если лут уже подобран игроком (чтобы EndPlay не уничтожил отданный предмет).
	bool bCollected = false;
};
