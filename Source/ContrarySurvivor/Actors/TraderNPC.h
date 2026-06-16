// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"
#include "AConsumableItem.h" // EConsumableType (тип расходника для товара)
#include "InteractableNPCInterface.h" // HUD-маркер находимости
#include "TraderNPC.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class AMasterInventoryItem;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Вид товара в магазине: предмет в рюкзак или пополнение патронов.
UENUM(BlueprintType)
enum class EShopEntryKind : uint8
{
	Item UMETA(DisplayName = "Item"),  // спавн предмета ItemClass -> в рюкзак
	Ammo UMETA(DisplayName = "Ammo")   // +AmmoAmount к резерву дальнобойного оружия игрока
};

/**
 * Позиция в прайс-листе торговца (GDD §7.6 — цены DRAFT на тюнинг).
 * Каталог торговца — массив таких записей; задаётся в конструкторе ATraderNPC (editor-
 * независимо) и тюнингуется в редакторе.
 */
USTRUCT(BlueprintType)
struct FShopEntry
{
	GENERATED_BODY()

	// Отображаемое имя в магазине.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	FString DisplayName;

	// Цена покупки (валюта). DRAFT по GDD §7.6.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	float Price = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	EShopEntryKind Kind = EShopEntryKind::Item;

	// Для Kind=Item: класс выдаваемого предмета (расходник/броня/оружие как inventory-item).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	TSubclassOf<AMasterInventoryItem> ItemClass;

	// Если выдаётся расходник — выставить ему этот тип (еда/вода/аптечка).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	bool bApplyConsumableType = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	EConsumableType ConsumableType = EConsumableType::Food;

	// Для Kind=Ammo: сколько патронов добавить в резерв.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop")
	int32 AmmoAmount = 0;
};

/**
 * NPC-торговец (Фаза 4, GDD §7.1: «в MVP 1 торговец»).
 *
 * НЕ враждебный и НЕ цель авто-лока. Решение «фильтра фракций» (тех-долг прошлых фаз):
 * торговец — обычный AActor (НЕ Pawn) и НЕ несёт UStatsComponent. Авто-лок/HUD-хелсбары
 * игрока итерируют ТОЛЬКО TActorIterator<APawn> с UStatsComponent, поэтому торговец не
 * попадает в выборку ни по типу (не Pawn), ни по признаку (нет Stats) — двойная защита.
 * Меш без коллизии по Visibility, так что луч дальнобоя сквозь него проходит, урон не идёт.
 *
 * Взаимодействие: при входе игрока в радиус (overlap) торговец регистрируется у
 * AContrarySurvivorPlayerController (NearbyTrader). Клавиша Interact открывает магазин
 * (immediate-mode экран на AContrarySurvivorHUD). Ставится актёром на уровень в редакторе.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API ATraderNPC : public AActor, public IInteractableNPCInterface
{
	GENERATED_BODY()

public:
	ATraderNPC();

	// Каталог товаров (для отрисовки магазина и покупки).
	const TArray<FShopEntry>& GetCatalog() const { return Catalog; }

	// Цена выкупа предмета у игрока (DRAFT ~50% от условной цены, по категории).
	float GetSellValue(const AMasterInventoryItem* Item) const;

	// Цена выкупа ОДНОГО патрона (для слайдера продажи стака патронов). DRAFT ~50% от цены покупки.
	float GetAmmoSellPerRound() const { return SellValueAmmoPerRound; }

	// --- IInteractableNPCInterface (HUD-маркер находимости) ---
	virtual FString GetNPCMarkerLabel() const override { return TEXT("Trader"); }
	virtual float GetNPCMarkerZOffset() const override { return 320.0f; }

protected:
	// Создаёт ЯРКИЙ плейсхолдер-материал (dynamic instance) и применяет к телу/голове,
	// чтобы торговца было заметно в деревне (placeholder до реального скина).
	virtual void BeginPlay() override;
	// Триггер диалоговой зоны: overlap по Pawn (игроку).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trader")
	USphereComponent* InteractTrigger;

	// Визуал торговца: скелет-меш на общем гуманоидном скелете (Head_Skeleton) + AnimBP
	// ABP_HumanoidCharacter. DRAFT: пока используем SK_Elder (отдельный меш торговца — финал-арт-пасс).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trader")
	USkeletalMeshComponent* CharMesh;

	// Плейсхолдер-меш тела (без коллизии). СКРЫТ (визуал-пасс): заменён на CharMesh.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trader")
	UStaticMeshComponent* MeshComponent;

	// Яркий «маяк»-шар над телом — заметная макушка, чтобы торговца было видно издалека.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trader")
	UStaticMeshComponent* BeaconComponent;

	// Базовый материал плейсхолдера (BasicShapeMaterial: имеет вектор-параметр "Color").
	// Грузится в конструкторе, в BeginPlay из него создаётся dynamic instance с яркой заливкой.
	UPROPERTY()
	UMaterialInterface* PlaceholderBaseMaterial = nullptr;

	// Яркий цвет плейсхолдера (DRAFT — заметный, до реального скина).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trader")
	FLinearColor PlaceholderColor = FLinearColor(1.0f, 0.15f, 0.85f, 1.0f); // ярко-маджента

	// Радиус, в котором доступно взаимодействие (см). DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trader")
	float InteractRadius = 220.0f;

	// Прайс-лист (заполняется дефолтами в конструкторе, тюнингуется в редакторе).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TArray<FShopEntry> Catalog;

	// Цены выкупа по категориям (DRAFT, ~50% от цены покупки соответствующего товара).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell")
	float SellValueConsumable = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell")
	float SellValueArmor = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell")
	float SellValueWeapon = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell")
	float SellValueResource = 2.0f;

	// Выкуп патронов — поштучно (стак). DRAFT ~50% от цены покупки (2/шт по GDD §7.6).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell")
	float SellValueAmmoPerRound = 1.0f;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	// Заполняет Catalog DRAFT-товарами по GDD §7.6 (вызывается в конструкторе).
	void BuildDefaultCatalog();
};
