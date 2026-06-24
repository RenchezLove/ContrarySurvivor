// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MasterHumanoidCharacter.h"                          // база (тот же каталог Characters/)
#include "ContrarySurvivor/Actors/InteractableNPCInterface.h" // HUD-маркер находимости
#include "ContrarySurvivor/Actors/ShopTypes.h"                // FShopEntry / EShopEntryKind (нейтральный тип, A2)
#include "ContrarySurvivor/Actors/ShopVendor.h"               // IShopVendor (магазин развязан от конкретного класса, A2)
#include "MasterTrader.generated.h"

class USphereComponent;
class AMasterInventoryItem;

/**
 * NPC-торговец на базе модульного гуманоида (Фаза «чистка деревни»).
 *
 * Это полноценный AMasterHumanoidCharacter (Pawn) и ЕДИНСТВЕННЫЙ вендор магазина: родитель
 * BP_Trader, реализует IShopVendor (каталог/цены). До A2 существовал старый ATraderNPC
 * (обычный AActor, не Pawn), под который были жёстко типизированы PlayerController/HUD; в A2
 * он удалён, а магазин развязан от класса через интерфейс IShopVendor.
 *
 * --- Почему стрельба игрока остаётся «как раньше» (подтверждено исходниками) ---
 *  1) НЕ несёт UStatsComponent. Авто-лок и хелсбары игрока
 *     (AContrarySurvivorPlayerController::FindNearestLivingTarget / IsValidTarget) отбирают
 *     цели через FindComponentByClass<UStatsComponent>(). Без компонента торговец НИКОГДА
 *     не становится авто-целью → оружие не получает его как Target.
 *  2) Капсулу НЕ переводим в Block по ECC_Visibility. Профиль Pawn по умолчанию ИГНОРИРУЕТ
 *     Visibility (ср. AEnemyCharacter «ПРАВКА A», где Block выставляют ЯВНО, чтобы враг стал
 *     простреливаемым). ARangedWeapon трассирует LineTraceSingleByChannel по ECC_Visibility —
 *     луч проходит сквозь торговца, как и сквозь старый ATraderNPC (там меши без коллизии).
 *
 * --- «Огромное здоровье» собственными средствами (без правок базы/игрока/оружия) ---
 *  Базовый AMasterHumanoidCharacter уже держит инлайн Health/MaxHealth и TakeDamage с путём
 *  смерти. Здесь TraderMaxHealth (UPROPERTY) задаёт огромный запас, а override TakeDamage
 *  НЕ зовёт базовый TakeDamage (минуя HandleDeath) и упирает HP в нижний порог TraderMinHealth —
 *  торговец математически неубиваем. UStatsComponent для этого НЕ нужен (и вреден, см. п.1).
 *
 * Реализует IInteractableNPCInterface → HUD сам рисует над ним маркер «Trader».
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AMasterTrader : public AMasterHumanoidCharacter, public IInteractableNPCInterface, public IShopVendor
{
	GENERATED_BODY()

public:
	AMasterTrader();

	// --- IShopVendor (магазин: каталог/цены; вызывается из PlayerController и HUD) ---

	// Каталог товаров (для отрисовки магазина и покупки).
	virtual const TArray<FShopEntry>& GetCatalog() const override { return Catalog; }

	// Цена выкупа предмета у игрока (DRAFT ~50% от цены покупки, по категории).
	virtual float GetSellValue(const AMasterInventoryItem* Item) const override;

	// Цена выкупа ОДНОГО патрона (для слайдера продажи стака патронов).
	virtual float GetAmmoSellPerRound() const override { return SellValueAmmoPerRound; }

	// --- IInteractableNPCInterface (HUD-маркер находимости) ---
	virtual FString GetNPCMarkerLabel() const override { return TEXT("Trader"); }
	virtual float GetNPCMarkerZOffset() const override { return 320.0f; }

	// --- Урон: неубиваемость собственными средствами (override, БЕЗ повтора UFUNCTION-макроса) ---
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	// Триггер диалоговой зоны: overlap по Pawn (игроку). По образцу ATraderNPC::InteractTrigger.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trader")
	USphereComponent* InteractTrigger;

	// Радиус, в котором доступно взаимодействие (см). DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trader")
	float InteractRadius = 220.0f;

	// Игрок сейчас в радиусе взаимодействия (выставляется overlap'ом). Для будущей привязки
	// открытия магазина и BP-логики.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trader")
	bool bPlayerInRange = false;

	// --- «Огромное здоровье» (собственный knob торговца; UStatsComponent НЕ используется) ---

	// Огромный запас HP. Применяется к инлайн Health/MaxHealth базы в BeginPlay (после
	// дефолтов BP). DRAFT 1e6 — на тюнинг из Class Defaults BP_Trader.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trader|Health", meta = (ClampMin = "1.0"))
	float TraderMaxHealth = 1000000.0f;

	// Нижний порог HP: TakeDamage никогда не опускает Health ниже него → гарантия не-смерти.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trader|Health", meta = (ClampMin = "1.0"))
	float TraderMinHealth = 1.0f;

	// --- Прайс-лист и выкуп (как у ATraderNPC, GDD §7.6 — DRAFT на тюнинг) ---

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TArray<FShopEntry> Catalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell")
	float SellValueConsumable = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell")
	float SellValueArmor = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell")
	float SellValueWeapon = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell")
	float SellValueResource = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell")
	float SellValueAmmoPerRound = 1.0f;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	// Заполняет Catalog DRAFT-товарами по GDD §7.6 (вызывается в конструкторе). Перенос из ATraderNPC.
	void BuildDefaultCatalog();

	// Применяет TraderMaxHealth к инлайн Health/MaxHealth базы (огромный запас).
	void ApplyTraderHealth();
};
