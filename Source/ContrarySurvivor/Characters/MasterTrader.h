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

	// Цена выкупа предмета у игрока. Товар прайс-листа выкупается за долю СВОЕЙ цены покупки
	// (BuybackPriceFraction, по умолчанию половина) — поэтому продать дороже, чем купил,
	// нельзя ни на одном товаре. Вещи вне прайс-листа (лут, квестовые) идут по категории.
	virtual float GetSellValue(const AMasterInventoryItem* Item) const override;

	// Цена выкупа ОДНОГО патрона (для слайдера продажи стака патронов).
	virtual float GetAmmoSellPerRound() const override { return SellValueAmmoPerRound; }

	// --- IInteractableNPCInterface (HUD-маркер находимости) ---
	virtual FText GetNPCMarkerLabel() const override { return NPCMarkerLabel; }
	virtual float GetNPCMarkerZOffset() const override { return NPCMarkerZOffset; }

	// --- Урон: неубиваемость собственными средствами (override, БЕЗ повтора UFUNCTION-макроса) ---
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	// Триггер диалоговой зоны: overlap по Pawn (игроку). По образцу ATraderNPC::InteractTrigger.
	// meta DisplayPriority — поднять наши настройки наверх Details (фидбек Рината), сразу после Transform.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trader", meta = (DisplayPriority = "1"))
	USphereComponent* InteractTrigger;

	// Радиус, в котором доступно взаимодействие (см). DRAFT.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trader")
	float InteractRadius = 220.0f;

	// Подпись и подъём HUD-маркера находимости (были зашиты в override интерфейса;
	// директива Рината 07-18: настраиваются на размещённом экземпляре BP_Trader).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trader")
	FText NPCMarkerLabel = NSLOCTEXT("NPC", "TraderMarker", "Торговец");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trader")
	float NPCMarkerZOffset = 320.0f;

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

	// Каталог пересобирается в BeginPlay (RebuildCatalog) с ценами Price* ниже — так работает
	// настройка на РАЗМЕЩЁННОМ экземпляре BP_Trader (значения из конструктора её бы не видели).
	// Ручные правки строк каталога в редакторе рантайм не переживают — цены крутить через Price*.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (DisplayPriority = "2"))
	TArray<FShopEntry> Catalog;

	// Цена СЛОТА брони по тирам (ADR-042, Ринат 2026-07-11: Т1≈50 / Т2≈120 / Т3≈250 за слот).
	// Тюнинг из редактора без пересборки; применяется ко всем трём слотам тира в RebuildCatalog.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (DisplayPriority = "3", ClampMin = "0.0"))
	float PriceArmorT1 = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (DisplayPriority = "4", ClampMin = "0.0"))
	float PriceArmorT2 = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shop", meta = (DisplayPriority = "5", ClampMin = "0.0"))
	float PriceArmorT3 = 250.0f;

	// ДОЛЯ цены прайс-листа, по которой торговец ВЫКУПАЕТ у игрока предмет, стоящий в этом
	// же прайс-листе (Build 1.2.2, 05-08: закрытие дыры в экономике). Раньше цена выкупа
	// бралась только по категории и с ценой покупки никак не сходилась: вода покупалась за
	// 5, а выкупалась за 6, нож покупался за 40 и выкупался за 70 — деньги печатались
	// покупкой и мгновенной продажей. Теперь цена выкупа считается от цены покупки ТОГО ЖЕ
	// предмета и потому всегда строго ниже неё. Потолок 0.9 намеренный: доля 1.0 вернула бы
	// дыру. Итог округляется ВНИЗ, чтобы число в окне совпадало с начисленным.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Sell",
		meta = (ClampMin = "0.0", ClampMax = "0.9", DisplayPriority = "1"))
	float BuybackPriceFraction = 0.5f;

	// Цены выкупа ПО КАТЕГОРИИ — запасной путь для вещей, которых в прайс-листе нет
	// (лут бандитов, квестовые предметы). Для товаров прайс-листа они больше не работают:
	// там цену задаёт BuybackPriceFraction от цены покупки.
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

	// Позиция прайс-листа, которой соответствует предмет игрока (по классу, а у расходников
	// ещё и по типу: вода/консервы/бинт стоят в каталоге одним классом). Наследник класса
	// позиции (например BP_Pistol от APistol) тоже считается совпадением — из подходящих
	// берётся САМАЯ ДЕШЁВАЯ позиция, чтобы выкуп не оказался выгоднее покупки. Нет
	// совпадения (лут, квестовые вещи) — nullptr, и цена берётся по категории.
	const FShopEntry* FindCatalogEntryForItem(const AMasterInventoryItem* Item) const;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	// (Пере)заполняет Catalog товарами по GDD §7.6 + броня Т1-Т3 по ценам Price* (ADR-042).
	// Зовётся из конструктора (дефолты CDO — видны в редакторе) И из BeginPlay (значения
	// с размещённого экземпляра; заодно перетирает устаревший сериализованный каталог BP).
	void RebuildCatalog();

	// Применяет TraderMaxHealth к инлайн Health/MaxHealth базы (огромный запас).
	void ApplyTraderHealth();
};
