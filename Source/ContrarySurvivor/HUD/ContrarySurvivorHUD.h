// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ContrarySurvivorHUD.generated.h"

class UStatsComponent;
class APlayerCharacter;
class AMasterInventoryItem;
class ATraderNPC;

// Тип действия кликабельной зоны инвентаря (Фаза 4). Immediate-mode UI: каждая зона
// хранит свой прямоугольник на экране и действие, выполняемое при клике мышью/тапе.
enum class EInvAction : uint8
{
	None,
	UnequipSlot,  // клик по слоту paper-doll с надетой бронёй -> снять (SlotIndex = EArmorSlot)
	UseItem,      // клик по предмету рюкзака -> использовать (надеть броню / съесть расходник)
	DropItem      // клик по кнопке [X] предмета -> выбросить
};

// Кликабельная зона инвентаря (пересобирается каждый кадр в DrawInventory). Не UObject-
// рефлексия: простая структура, указатель валидируется в обработчике (IsValid). Предметы
// удерживаются живыми инвентарём, поэтому переживают кадр между отрисовкой и кликом.
struct FInvHitRegion
{
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
	EInvAction Action = EInvAction::None;
	AMasterInventoryItem* Item = nullptr; // для UseItem/DropItem
	int32 SlotIndex = -1;                 // для UnequipSlot (приведение к EArmorSlot)
};

// Тип действия кликабельной зоны магазина (Фаза 4, экономика).
enum class EShopAction : uint8
{
	None,
	Buy,   // купить позицию каталога (EntryIndex)
	Sell,  // продать предмет рюкзака (Item)
	Close  // закрыть магазин
};

// Кликабельная зона магазина (пересобирается каждый кадр в DrawShop).
struct FShopHitRegion
{
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
	EShopAction Action = EShopAction::None;
	AMasterInventoryItem* Item = nullptr; // для Sell
	int32 EntryIndex = -1;                // для Buy (индекс в каталоге торговца)
};

/**
 * HUD первого вертикального среза (Фаза 1).
 * Рисует полоску здоровья над врагами прямо на Canvas (UMG-граф в UE 5.5 не редактируется
 * через Python, поэтому хелсбар сделан C++-отрисовкой на AHUD::DrawHUD — без ручных шагов).
 *
 * Показываем хелсбар врага (AEnemyCharacter), если он:
 *   - жив (StatsComponent не мёртв),
 *   - попадает в кадр (Project вернул точку перед камерой),
 *   - и (залочен игроком ИЛИ находится в радиусе HealthBarShowRadius от игрока) — GDD ч.8.
 *
 * Назначается как HUDClass в BP GameMode (делает unreal-operator).
 */
UCLASS()
class CONTRARYSURVIVOR_API AContrarySurvivorHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// --- Экран инвентаря (Фаза 4, GDD §7.4) — immediate-mode, без UMG/.uasset ---

	// Открыть/закрыть/переключить экран инвентаря. Вызывается контроллером по клавише.
	void SetInventoryOpen(bool bOpen);
	void ToggleInventory();
	bool IsInventoryOpen() const { return bInventoryOpen; }

	// Обработать клик мыши/тап по экрану инвентаря в экранных координатах ScreenPos.
	// Ищет попавшую кликабельную зону (зоны собраны прошлым DrawInventory) и выполняет
	// действие через APlayerCharacter (надеть/снять/использовать/выбросить).
	// Возвращает true, если зона найдена и действие выполнено.
	bool HandleInventoryClick(FVector2D ScreenPos);

	// --- Экран магазина (Фаза 4, экономика — GDD §7.6) — immediate-mode, без UMG/.uasset ---

	// Открыть/закрыть магазин конкретного торговца (вызывается контроллером по клавише).
	void SetShopOpen(bool bOpen, ATraderNPC* Trader);
	bool IsShopOpen() const { return bShopOpen; }

	// Обработать клик мыши/тап по экрану магазина (купить/продать/закрыть). Возвращает true,
	// если зона найдена и действие выполнено.
	bool HandleShopClick(FVector2D ScreenPos);

protected:
	// Радиус (в Unreal units), в пределах которого над врагом показывается хелсбар.
	// GDD ч.8: «при приближении ближе ~5 м». 5 м ≈ 500 ед, но для top-down-обзора берём с запасом.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	float HealthBarShowRadius = 1500.0f;

	// Размеры полоски здоровья в пикселях.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	float HealthBarWidth = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	float HealthBarHeight = 8.0f;

	// На сколько единиц над Actor location поднимаем якорь полоски (над головой).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	float HealthBarWorldZOffset = 110.0f;

	// Цвета.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	FLinearColor FillColor = FLinearColor(0.85f, 0.1f, 0.1f, 0.9f);

	// Цвет заполнения хелсбара ИМЕННО текущей залоченной цели (ярче обычного — выделяем).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	FLinearColor TargetFillColor = FLinearColor(1.0f, 0.25f, 0.1f, 1.0f);

	// --- Маркер ТЕКУЩЕЙ залоченной цели (ФИКС1: игрок должен видеть, кого бьёт) ---

	// На сколько единиц над Actor location поднимаем якорь маркера (центр силуэта цели).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|TargetMarker")
	float TargetMarkerWorldZOffset = 50.0f;

	// Полуразмер рамки-ретикла (px): угловые скобки рисуются по углам квадрата 2*HalfSize.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|TargetMarker")
	float TargetMarkerHalfSize = 46.0f;

	// Длина «плеча» угловой скобки (px).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|TargetMarker")
	float TargetMarkerCornerLen = 16.0f;

	// Толщина линий маркера (px).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|TargetMarker")
	float TargetMarkerThickness = 3.0f;

	// Высота указывающего вниз треугольника над рамкой (px) и зазор до рамки.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|TargetMarker")
	float TargetMarkerTriHeight = 18.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|TargetMarker")
	float TargetMarkerTriGap = 6.0f;

	// Цвет маркера цели — заметный (жёлтый).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|TargetMarker")
	FLinearColor TargetMarkerColor = FLinearColor(1.0f, 0.92f, 0.1f, 1.0f);

	// --- HUD игрока (GDD §7.7) ---

	// Левый верхний угол: отступы и размеры HP-бара игрока.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	float PlayerHudMarginX = 24.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	float PlayerHudMarginY = 24.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	float PlayerHealthBarWidth = 260.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	float PlayerHealthBarHeight = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	FLinearColor PlayerHealthFillColor = FLinearColor(0.85f, 0.1f, 0.1f, 0.95f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	FLinearColor HungerColor = FLinearColor(0.85f, 0.55f, 0.1f, 0.95f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	FLinearColor ThirstColor = FLinearColor(0.15f, 0.55f, 0.9f, 0.95f);

	// --- Экран инвентаря (GDD §7.4) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Inventory")
	FLinearColor InvDimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);      // затемнение фона

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Inventory")
	FLinearColor InvPanelColor = FLinearColor(0.06f, 0.07f, 0.09f, 0.95f); // фон панели

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Inventory")
	FLinearColor InvSlotColor = FLinearColor(0.15f, 0.16f, 0.2f, 1.0f);    // пустой слот/строка

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Inventory")
	FLinearColor InvSlotFilledColor = FLinearColor(0.2f, 0.3f, 0.22f, 1.0f); // занятый слот

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Inventory")
	FLinearColor InvHoverColor = FLinearColor(0.32f, 0.38f, 0.5f, 1.0f);   // подсветка под курсором

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Inventory")
	FLinearColor InvDropColor = FLinearColor(0.5f, 0.12f, 0.12f, 1.0f);    // кнопка [X] выброса

	// --- Контекстная подсказка взаимодействия (E) — BUG3 ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Interact")
	FLinearColor InteractPromptBgColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Interact")
	FLinearColor InteractPromptTextColor = FLinearColor(1.0f, 0.95f, 0.5f, 1.0f);

private:
	// Рисует одну полоску здоровья над целью ЛЮБОГО типа (бандит/волк/любой враг
	// с UStatsComponent). Тип-агностично: принимает актёра и его компонент статов.
	// bIsCurrentTarget — текущая залоченная цель (рисуется ярким TargetFillColor).
	void DrawTargetHealthBar(AActor* TargetActor, UStatsComponent* Stats, bool bIsCurrentTarget);

	// Рисует маркер-ретикл (угловые скобки + указатель) над ТЕКУЩЕЙ залоченной целью,
	// чтобы игрок чётко видел активную цель (ФИКС1). Через DrawHUD, без UMG.
	void DrawTargetMarker(AActor* TargetActor);

	// Рисует статы игрока (HP-бар слева вверху, критич. голод/жажда под ним, деньги).
	void DrawPlayerStats(UStatsComponent* Stats);

	// Рисует контекстную подсказку взаимодействия («E — подобрать» / «E — торговать»)
	// по центру снизу (BUG3). Текст берётся у контроллера (ближайший интерактив).
	void DrawInteractPrompt(const FString& Text);

	// --- Экран инвентаря (immediate-mode) ---

	// Открыт ли экран инвентаря (модальный поверх HUD).
	bool bInventoryOpen = false;

	// Кликабельные зоны, пересобираемые каждый DrawInventory. Используются HandleInventoryClick.
	TArray<FInvHitRegion> InvHitRegions;

	// Рисует весь экран инвентаря: слева paper-doll (Head/Torso/Legs + оружие),
	// справа рюкзак (неэкипированные предметы), сверху деньги/голод/жажда. Заполняет InvHitRegions.
	void DrawInventory(APlayerCharacter* Player);

	// Рисует прямоугольную «плитку» (фон + опц. подсветка под курсором) и текст. Хелпер layout.
	void DrawInvBox(float X, float Y, float W, float H, const FLinearColor& BaseColor,
		const FVector2D& MousePos, const FString& Label, class UFont* Font);

	// Точка внутри прямоугольника зоны?
	static bool PointInRegion(const FVector2D& P, const FInvHitRegion& R);

	// --- Экран магазина (immediate-mode) ---

	bool bShopOpen = false;

	// Торговец, чей каталог отрисовываем (источник цен/товаров).
	UPROPERTY()
	ATraderNPC* ShopTrader = nullptr;

	// Кликабельные зоны магазина, пересобираются каждый DrawShop.
	TArray<FShopHitRegion> ShopHitRegions;

	// Рисует экран магазина: слева каталог (товары+цены+[buy]), справа рюкзак (предметы+[sell]),
	// сверху деньги + [Close]. Заполняет ShopHitRegions.
	void DrawShop(APlayerCharacter* Player);
};
