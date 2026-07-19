// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ContrarySurvivor/Actors/ShopVendor.h" // IShopVendor (источник каталога/цен магазина, A2)
#include "ContrarySurvivorHUD.generated.h"

class UStatsComponent;
class UQuestComponent;
class APlayerCharacter;
class AMasterInventoryItem;
class AElderNPC;
class UTexture2D;
class UShopScreenWidget;
class UDialogScreenWidget;
class UInventoryScreenWidget;
class UDeathScreenWidget;
class UPlayerStatsWidget;
class UQuestTrackerWidget;
class UInteractPromptWidget;

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

// Тип действия кликабельной зоны магазина (Фаза 4, экономика; Фаза 5 — слайдер количества).
enum class EShopAction : uint8
{
	None,
	Buy,            // арм слайдера покупки позиции каталога (EntryIndex)
	Sell,           // арм слайдера/прямая продажа предмета рюкзака (Item)
	Close,          // закрыть магазин
	SliderTrack,    // клик по треку слайдера -> qty = по позиции мыши
	SliderDec,      // кнопка [-]
	SliderInc,      // кнопка [+]
	SliderConfirm,  // подтвердить покупку/продажу на выбранное qty
	SliderCancel    // отменить слайдер (вернуться к списку)
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

// Зона магазина, в которой НАЧАЛСЯ жест пальца (тач-прокрутка, этап G2). Зона фиксируется
// в момент превышения порога свайпа и не меняется до отпускания пальца — палец может
// уходить за границы списка, прокрутка продолжается.
enum class EShopDragZone : uint8
{
	None,
	BuyList,     // левая колонка «FOR SALE» — свайп листает каталог
	SellList,    // правая колонка «SELL FROM BACKPACK» — свайп листает рюкзак
	SliderTrack  // трек слайдера количества — свайп по X выбирает количество
};

// Тип действия кликабельной зоны диалога (Фаза 5, квесты).
enum class EDialogAction : uint8
{
	None,
	Accept,   // принять предложенный квест
	Decline,  // отказаться (закрыть, не принимая)
	TurnIn,   // сдать выполненный квест (получить награду)
	Close     // закрыть диалог (после принятия/сдачи)
};

// Кликабельная зона диалога (пересобирается каждый кадр в DrawDialog).
struct FDialogHitRegion
{
	FVector2D Min = FVector2D::ZeroVector;
	FVector2D Max = FVector2D::ZeroVector;
	EDialogAction Action = EDialogAction::None;
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

	// ADR-048: постоянные UMG-панели (статы игрока / трекер квеста / подсказка E)
	// создаются один раз на старте, если их слоты назначены.
	virtual void BeginPlay() override;

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

	// Открыть/закрыть магазин конкретного вендора (вызывается контроллером по клавише).
	void SetShopOpen(bool bOpen, TScriptInterface<IShopVendor> Trader);
	bool IsShopOpen() const { return bShopOpen; }

	// Обработать клик мыши/тап по экрану магазина (купить/продать/закрыть). Возвращает true,
	// если зона найдена и действие выполнено.
	bool HandleShopClick(FVector2D ScreenPos);

	// Слайдер количества активен (открыта транзакция купли/продажи стака)? Контроллер маршрутит
	// клавиши ±количества только когда true.
	bool IsShopSliderActive() const { return bSliderActive; }

	// Изменить выбранное количество слайдера на Delta (клавиши ±1 / Shift ±10 / колесо). Кламп 1..max.
	void AdjustShopSliderQty(int32 Delta);

	// Прокрутить список магазина на Delta строк (колесо/стрелки при НЕактивном слайдере —
	// те же экшены ShopQtyInc/Dec, маршрутит контроллер). Листается колонка ПОД КУРСОРОМ:
	// над рюкзаком — правый список (G2: рюкзак тоже перерастает панель), иначе каталог
	// (прежнее поведение). Кламп 0..max, max пересчитывается в DrawShop.
	void ScrollShopList(int32 DeltaRows);

	// --- Тач-жесты магазина (этап G2) — зовёт контроллер из BindTouch-обработчиков ---

	// В какой зоне магазина лежит точка (по прямоугольникам последнего DrawShop). При
	// активном слайдере списки закрыты модально — отвечает только SliderTrack/None.
	EShopDragZone GetShopDragZone(FVector2D ScreenPos) const;

	// Свайп-прокрутка списка зоны (каталог/рюкзак) на DeltaPixels по вертикали. Пиксели
	// накапливаются между кадрами и конвертируются в строки по фактическому шагу строки
	// последнего DrawShop; положительная дельта листает список вниз (палец ведут вверх).
	void ScrollShopZonePixels(EShopDragZone Zone, float DeltaPixels);

	// Свайп по треку слайдера: выставить количество по экранной X — та же математика,
	// что у клика по треку в HandleShopClick (палец тянет ручку непрерывно).
	void SetShopSliderQtyFromX(float ScreenX);

	// Выполнить транзакцию на выбранное qty и закрыть слайдер (Enter/кнопка Confirm).
	void ConfirmShopSlider(APlayerCharacter* Player);

	// --- Экран диалога со старостой (Фаза 5, квесты — GDD §7.7) — immediate-mode, без UMG ---

	// Открыть/закрыть диалог с конкретным старостой (вызывается контроллером по клавише E).
	void SetDialogOpen(bool bOpen, AElderNPC* Elder);
	bool IsDialogOpen() const { return bDialogOpen; }

	// Обработать клик/тап по экрану диалога (принять/отказаться/сдать/закрыть). Возвращает true,
	// если зона найдена и действие выполнено.
	bool HandleDialogClick(FVector2D ScreenPos);

	// --- Экран смерти (#26) — immediate-mode, без UMG ---

	// Показать/скрыть экран смерти (вызывается контроллером из ShowDeathScreen/HideDeathScreen).
	void SetDeathScreenOpen(bool bOpen);
	bool IsDeathScreenOpen() const { return bDeathScreen; }

	// Обработать клик/тап по экрану смерти. Возвращает true, если попали в кнопку «Возродиться»
	// (тогда контроллер запускает респаун). Сам респаун HUD не делает (логика — у игрока).
	bool HandleDeathScreenClick(FVector2D ScreenPos);

	// --- Всплывающие цифры урона (D5) — ТОЛЬКО по врагам (решение Рината) ---

	// Зарегистрировать цифру урона над мировой точкой (зовут TakeDamage бандита/волка).
	// Рисуется DrawDamageNumbers: поднимается и гаснет за DamageNumberLifetime.
	void AddDamageNumber(const FVector& WorldLocation, float Amount);

protected:
	// Радиус (в Unreal units), в пределах которого над врагом показывается хелсбар.
	// GDD ч.8: «при приближении ближе ~5 м». 5 м ≈ 500 ед, но для top-down-обзора берём с запасом.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|HealthBar", meta = (DisplayPriority = "1"))
	float HealthBarShowRadius = 1500.0f;

	// Размеры полоски здоровья в пикселях.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|HealthBar", meta = (DisplayPriority = "1"))
	float HealthBarWidth = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|HealthBar", meta = (DisplayPriority = "1"))
	float HealthBarHeight = 8.0f;

	// На сколько единиц над Actor location поднимаем якорь полоски (над головой).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|HealthBar", meta = (DisplayPriority = "1"))
	float HealthBarWorldZOffset = 110.0f;

	// Цвета.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|HealthBar", meta = (DisplayPriority = "1"))
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|HealthBar", meta = (DisplayPriority = "1"))
	FLinearColor FillColor = FLinearColor(0.85f, 0.1f, 0.1f, 0.9f);

	// Цвет заполнения хелсбара ИМЕННО текущей залоченной цели (ярче обычного — выделяем).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|HealthBar", meta = (DisplayPriority = "1"))
	FLinearColor TargetFillColor = FLinearColor(1.0f, 0.25f, 0.1f, 1.0f);

	// --- Маркер ТЕКУЩЕЙ залоченной цели (ФИКС1: игрок должен видеть, кого бьёт) ---

	// На сколько единиц над Actor location поднимаем якорь маркера (центр силуэта цели).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|TargetMarker", meta = (DisplayPriority = "1"))
	float TargetMarkerWorldZOffset = 50.0f;

	// Полуразмер рамки-ретикла (px): угловые скобки рисуются по углам квадрата 2*HalfSize.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|TargetMarker", meta = (DisplayPriority = "1"))
	float TargetMarkerHalfSize = 46.0f;

	// Длина «плеча» угловой скобки (px).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|TargetMarker", meta = (DisplayPriority = "1"))
	float TargetMarkerCornerLen = 16.0f;

	// Толщина линий маркера (px).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|TargetMarker", meta = (DisplayPriority = "1"))
	float TargetMarkerThickness = 3.0f;

	// Высота указывающего вниз треугольника над рамкой (px) и зазор до рамки.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|TargetMarker", meta = (DisplayPriority = "1"))
	float TargetMarkerTriHeight = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|TargetMarker", meta = (DisplayPriority = "1"))
	float TargetMarkerTriGap = 6.0f;

	// Цвет маркера цели — заметный (жёлтый).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|TargetMarker", meta = (DisplayPriority = "1"))
	FLinearColor TargetMarkerColor = FLinearColor(1.0f, 0.92f, 0.1f, 1.0f);

	// --- Маркер интерактивных NPC (торговец, позже староста) — находимость ---
	// Отличается от маркера ВРАГА (жёлтый ретикл): иной цвет (зелёный) и форма (ромб),
	// + стрелка по краю экрана, если NPC за кадром.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|NPCMarker", meta = (DisplayPriority = "1"))
	FLinearColor NPCMarkerColor = FLinearColor(0.15f, 0.95f, 0.45f, 1.0f); // зелёный (дружественный)

	// Полуразмер ромба маркера на экране (px), когда NPC в кадре.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|NPCMarker", meta = (DisplayPriority = "1"))
	float NPCMarkerHalfSize = 16.0f;

	// Толщина линий маркера/стрелки (px).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|NPCMarker", meta = (DisplayPriority = "1"))
	float NPCMarkerThickness = 3.0f;

	// Отступ от края экрана для зажатой к краю стрелки (px).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|NPCMarker", meta = (DisplayPriority = "1"))
	float NPCMarkerEdgeMargin = 56.0f;

	// Длина (px) указывающей стрелки за кадром.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|NPCMarker", meta = (DisplayPriority = "1"))
	float NPCMarkerArrowLen = 22.0f;

	// --- Всплывающие цифры урона (D5). Директива Рината 06-25: EditAnywhere+BRW, наверх. ---

	// Время жизни цифры (сек): подъём + затухание.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|DamageNumbers", meta = (ClampMin = "0.1", DisplayPriority = "1"))
	float DamageNumberLifetime = 0.8f;

	// Скорость подъёма цифры (мировых см/сек вверх от точки попадания).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|DamageNumbers", meta = (ClampMin = "0.0", DisplayPriority = "2"))
	float DamageNumberRiseSpeed = 110.0f;

	// Стартовая высота цифры над точкой попадания (см; над головой врага).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|DamageNumbers", meta = (DisplayPriority = "3"))
	float DamageNumberZOffset = 130.0f;

	// Масштаб текста цифры (растровый шрифт HUD).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|DamageNumbers", meta = (ClampMin = "0.5", DisplayPriority = "4"))
	float DamageNumberTextScale = 1.25f;

	// Цвет цифры урона.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|DamageNumbers", meta = (DisplayPriority = "5"))
	FLinearColor DamageNumberColor = FLinearColor(1.0f, 0.9f, 0.35f, 1.0f);

	// --- Краевые стрелки на стрелков за кадром (D6, ADR-035) ---

	// Цвет стрелки на ВРАГА-стрелка за кадром (враждебный красный; NPC-стрелки зелёные).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|EnemyArrow", meta = (DisplayPriority = "6"))
	FLinearColor EnemyShooterArrowColor = FLinearColor(1.0f, 0.15f, 0.1f, 1.0f);

	// --- Метка цели активного квеста (Этап D, реюз маркеров NPC) ---

	// Цвет метки цели квеста (золотой — в тон трекеру квеста).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|QuestMarker", meta = (DisplayPriority = "7"))
	FLinearColor QuestTargetMarkerColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);

	// Подъём якоря метки над актором-целью (см) — выше визуализаторов базы.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|QuestMarker", meta = (DisplayPriority = "8"))
	float QuestTargetMarkerZOffset = 300.0f;

	// --- HUD игрока (GDD §7.7) ---

	// Левый верхний угол: отступы и размеры HP-бара игрока.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player", meta = (DisplayPriority = "1"))
	float PlayerHudMarginX = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player", meta = (DisplayPriority = "1"))
	float PlayerHudMarginY = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player", meta = (DisplayPriority = "1"))
	float PlayerHealthBarWidth = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player", meta = (DisplayPriority = "1"))
	float PlayerHealthBarHeight = 28.0f;

	// Высота баров голода/жажды (#18: крупнее для читаемости).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player", meta = (DisplayPriority = "1"))
	float PlayerSurvivalBarHeight = 24.0f;

	// Подложка-плашка под текстом денег (#18) — тёмный полупрозрачный прямоугольник.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player", meta = (DisplayPriority = "1"))
	FLinearColor MoneyPlateColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	// Цвет текста патронов экипированного оружия (#5).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player", meta = (DisplayPriority = "1"))
	FLinearColor AmmoColor = FLinearColor(0.95f, 0.95f, 0.95f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player", meta = (DisplayPriority = "1"))
	FLinearColor PlayerHealthFillColor = FLinearColor(0.85f, 0.1f, 0.1f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player", meta = (DisplayPriority = "1"))
	FLinearColor HungerColor = FLinearColor(0.85f, 0.55f, 0.1f, 0.95f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player", meta = (DisplayPriority = "1"))
	FLinearColor ThirstColor = FLinearColor(0.15f, 0.55f, 0.9f, 0.95f);

	// --- Экран инвентаря (GDD §7.4) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory", meta = (DisplayPriority = "1"))
	FLinearColor InvDimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);      // затемнение фона

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory", meta = (DisplayPriority = "1"))
	FLinearColor InvPanelColor = FLinearColor(0.06f, 0.07f, 0.09f, 0.95f); // фон панели

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory", meta = (DisplayPriority = "1"))
	FLinearColor InvSlotColor = FLinearColor(0.15f, 0.16f, 0.2f, 1.0f);    // пустой слот/строка

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory", meta = (DisplayPriority = "1"))
	FLinearColor InvSlotFilledColor = FLinearColor(0.2f, 0.3f, 0.22f, 1.0f); // занятый слот

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory", meta = (DisplayPriority = "1"))
	FLinearColor InvHoverColor = FLinearColor(0.32f, 0.38f, 0.5f, 1.0f);   // подсветка под курсором

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory", meta = (DisplayPriority = "1"))
	FLinearColor InvDropColor = FLinearColor(0.5f, 0.12f, 0.12f, 1.0f);    // кнопка [X] выброса

	// --- Иконки слотов брони (ADR-043) ---
	// Иконка ПУСТОГО слота paper-doll (читается как пустой). МЯГКИЕ ссылки: текстур может
	// ещё не быть в проекте (импорт по этим именам позже) — тогда текстовый фолбэк «(пусто)»,
	// без крашей и без повторных попыток загрузки каждый кадр (см. ResolveIcon/IconCache).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory", meta = (DisplayPriority = "2"))
	TSoftObjectPtr<UTexture2D> EmptySlotIconHead =
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Icons/T_Icon_Slot_Head.T_Icon_Slot_Head")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory", meta = (DisplayPriority = "2"))
	TSoftObjectPtr<UTexture2D> EmptySlotIconTorso =
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Icons/T_Icon_Slot_Torso.T_Icon_Slot_Torso")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory", meta = (DisplayPriority = "2"))
	TSoftObjectPtr<UTexture2D> EmptySlotIconLegs =
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Icons/T_Icon_Slot_Legs.T_Icon_Slot_Legs")));

	// --- Контекстная подсказка взаимодействия (E) — BUG3 ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Interact", meta = (DisplayPriority = "1"))
	FLinearColor InteractPromptBgColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.65f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Interact", meta = (DisplayPriority = "1"))
	FLinearColor InteractPromptTextColor = FLinearColor(1.0f, 0.95f, 0.5f, 1.0f);

	// --- Трекер активного квеста (Фаза 5, GDD §7.7) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Quest", meta = (DisplayPriority = "1"))
	FLinearColor QuestTrackerColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Quest", meta = (DisplayPriority = "1"))
	FLinearColor QuestTrackerDoneColor = FLinearColor(0.4f, 1.0f, 0.4f, 1.0f);

	// --- Читаемость HUD магазина/инвентаря (#18): жирный/крупный текст, подложки, обводка ---
	// Вынесено в EditAnywhere, чтобы Ринат твикал размеры/цвета без пересборки.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	float UIHeaderTextScale = 1.3f;        // заголовки панелей (INVENTORY/TRADER/...)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	float UISubHeaderTextScale = 1.12f;    // подзаголовки колонок (EQUIPMENT/FOR SALE/...)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	float UIMoneyTextScale = 1.25f;        // строка денег в панелях

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	float UIBoxLabelScale = 1.0f;          // подписи внутри плиток/кнопок (с обводкой)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	float UISliderTitleScale = 1.35f;      // заголовок слайдера BUY/SELL

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	float UISliderQtyScale = 1.5f;         // число количества в слайдере

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	float UISliderPriceScale = 1.4f;       // итоговая цена/выручка в слайдере

	// Подложка-плашка под ключевыми надписями (тёмная полупрозрачная).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	FLinearColor UITextPlateColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	// Светлый цвет заголовков/подписей.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	FLinearColor UIHeaderColor = FLinearColor(0.95f, 0.96f, 1.0f, 1.0f);

	// Золотой акцент для денег/цен.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	FLinearColor UIMoneyColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	// Рамка-обводка вокруг модальных панелей (золотой акцент).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	FLinearColor UIPanelBorderColor = FLinearColor(0.8f, 0.65f, 0.25f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Readability", meta = (DisplayPriority = "1"))
	float UIPanelBorderThickness = 2.0f;

	// ======================================================================
	// UMG-миграция панелей (ADR-048): слоты классов виджетов. Слот пуст — работает
	// СТАРЫЙ Canvas-путь панели; Ринат построил WBP по схеме из
	// docs/contrary-survivor/umg-layout-guide.md и назначил в слот — панель живёт
	// в UMG, её Canvas-код глушится. Игра играбельна при ЛЮБОЙ комбинации слотов.
	// ======================================================================

	// Экран магазина (WBP_Shop). Пусто — Canvas DrawShop как раньше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|UMG Widgets", meta = (DisplayPriority = "1"))
	TSubclassOf<UShopScreenWidget> ShopWidgetClass;

	// Экран диалога со старостой (WBP_Dialog). Пусто — Canvas DrawDialog как раньше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|UMG Widgets", meta = (DisplayPriority = "2"))
	TSubclassOf<UDialogScreenWidget> DialogWidgetClass;

	// Экран инвентаря (WBP_Inventory). Пусто — Canvas DrawInventory как раньше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|UMG Widgets", meta = (DisplayPriority = "3"))
	TSubclassOf<UInventoryScreenWidget> InventoryWidgetClass;

	// Экран смерти (WBP_Death). Пусто — Canvas DrawDeathScreen как раньше.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|UMG Widgets", meta = (DisplayPriority = "4"))
	TSubclassOf<UDeathScreenWidget> DeathWidgetClass;

	// Постоянные панели: статы игрока / трекер квеста / подсказка взаимодействия
	// (WBP_PlayerStats / WBP_QuestTracker / WBP_InteractPrompt). Пусто — Canvas.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|UMG Widgets", meta = (DisplayPriority = "5"))
	TSubclassOf<UPlayerStatsWidget> PlayerStatsWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|UMG Widgets", meta = (DisplayPriority = "6"))
	TSubclassOf<UQuestTrackerWidget> QuestTrackerWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|UMG Widgets", meta = (DisplayPriority = "7"))
	TSubclassOf<UInteractPromptWidget> InteractPromptWidgetClass;

	// ======================================================================
	// Настраиваемость из BP (директива Рината 07-18): геометрия панелей и ВСЕ тексты
	// вынесены в EditAnywhere-поля. Дефолты дословно повторяют прежние зашитые значения.
	// Формат-строки с параметрами вынесены полями Prefix/Suffix (решение game-lead:
	// сырые Printf-форматы в редактор не отдавать). Микро-отступы (6-30px внутренних
	// зазоров) сознательно оставлены в коде — иначе Details утонет в полях.
	// ПРИМЕЧАНИЕ ADR-048: поля панелей, переехавших на UMG, живут в классах виджетов
	// (одно место правды); Canvas-путь этих панелей вернулся к литералам (те же строки)
	// и целиком выпиливается после приёмки Рината.
	// ======================================================================

	// --- Общая геометрия центральных панелей (инвентарь И магазин — одинаковы по дизайну) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Panel Layout", meta = (DisplayPriority = "1"))
	float UIPanelMaxWidth = 960.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Panel Layout", meta = (DisplayPriority = "2"))
	float UIPanelMaxHeight = 600.0f;

	// Доля экрана, которую панель занимает на малых окнах (кламп сверху MaxWidth/MaxHeight).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Panel Layout", meta = (DisplayPriority = "3", ClampMin = "0.3", ClampMax = "1.0"))
	float UIPanelScreenFrac = 0.86f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Panel Layout", meta = (DisplayPriority = "4"))
	float UIPanelPadding = 16.0f;

	// Строки списков (каталог/рюкзак): высота и зазор.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Panel Layout", meta = (DisplayPriority = "5", ClampMin = "10.0"))
	float UIRowHeight = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Panel Layout", meta = (DisplayPriority = "6", ClampMin = "0.0"))
	float UIRowGap = 6.0f;

	// --- Магазин: геометрия ---

	// Доля ширины панели под левую колонку «FOR SALE».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Layout", meta = (DisplayPriority = "1", ClampMin = "0.2", ClampMax = "0.8"))
	float ShopLeftColumnFrac = 0.52f;

	// Кнопки Buy/Sell в строках списков.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Layout", meta = (DisplayPriority = "2"))
	float ShopRowButtonWidth = 64.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Layout", meta = (DisplayPriority = "3"))
	float ShopCloseButtonWidth = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Layout", meta = (DisplayPriority = "4"))
	float ShopCloseButtonHeight = 28.0f;

	// --- Магазин: тексты ---

	// Тексты Canvas-пути магазина (ADR-048: динамические строки UMG-пути переехали
	// в UShopScreenWidget — Монеты/Кол-во/Итого/Выручка/Buy/Sell/КУПИТЬ/ПРОДАТЬ;
	// здесь остались только Canvas-специфичные, умрут вместе с DrawShop).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Texts", meta = (DisplayPriority = "1"))
	FString ShopHeaderText = TEXT("TRADER  (E to close)");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Texts", meta = (DisplayPriority = "3"))
	FString ShopBuyHeaderText = TEXT("FOR SALE  (Купить)");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Texts", meta = (DisplayPriority = "4"))
	FString ShopSellHeaderText = TEXT("SELL FROM BACKPACK  (Продать)");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Texts", meta = (DisplayPriority = "7"))
	FString ShopCloseButtonText = TEXT("Close");

	// Хвост счётчика прокрутки «X-Y из N …»: на ПК — про колесо, на таче — про свайп
	// (выбор по наличию тач-слоя у контроллера, решение game-lead 07-18).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Texts", meta = (DisplayPriority = "8"))
	FString ShopScrollHintWheel = TEXT("(колесо — листать)");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Texts", meta = (DisplayPriority = "9"))
	FString ShopScrollHintSwipe = TEXT("(свайп — листать)");

	// --- Слайдер количества: геометрия ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "1"))
	float SliderPanelMaxWidth = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "2", ClampMin = "0.3", ClampMax = "1.0"))
	float SliderPanelScreenFrac = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "3"))
	float SliderPanelHeight = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "4"))
	float SliderPanelPadding = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "5"))
	float SliderTrackHeight = 10.0f;

	// Кнопки [-] [+].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "6"))
	float SliderSmallButtonWidth = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "7"))
	float SliderSmallButtonHeight = 30.0f;

	// Кнопки [Confirm]/[Cancel].
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "8"))
	float SliderBigButtonWidth = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "9"))
	float SliderBigButtonHeight = 34.0f;

	// Цвет строки «Кол-во: …» (светло-жёлтый) и подсказки клавиш внизу панели.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "10"))
	FLinearColor SliderQtyColor = FLinearColor(1.0f, 0.97f, 0.7f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider", meta = (DisplayPriority = "11"))
	FLinearColor SliderKeysHintColor = FLinearColor(0.8f, 0.8f, 0.82f, 1.0f);

	// --- Слайдер количества: тексты (Canvas-путь; КУПИТЬ/ПРОДАТЬ/Кол-во/Итого/Выручка
	// переехали в UShopScreenWidget — ADR-048) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider Texts", meta = (DisplayPriority = "6"))
	FString SliderConfirmText = TEXT("Confirm");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider Texts", meta = (DisplayPriority = "7"))
	FString SliderCancelText = TEXT("Cancel");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Shop Slider Texts", meta = (DisplayPriority = "8"))
	FString SliderKeysHintText = TEXT("[<-/->] +-1   [Shift] +-10   [Enter] confirm");

	// --- Инвентарь: геометрия ---

	// Доля ширины панели под левую колонку «СНАРЯЖЕНИЕ» (paper-doll).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Layout", meta = (DisplayPriority = "1", ClampMin = "0.2", ClampMax = "0.8"))
	float InvLeftColumnFrac = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Layout", meta = (DisplayPriority = "2"))
	float InvSlotHeight = 56.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Layout", meta = (DisplayPriority = "3"))
	float InvSlotGap = 10.0f;

	// Кнопка [X] выброса в строке рюкзака.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Layout", meta = (DisplayPriority = "4"))
	float InvDropButtonWidth = 30.0f;

	// Цвет строки-подсказки «(клик по занятому слоту — снять броню)».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Layout", meta = (DisplayPriority = "5"))
	FLinearColor InvHintTextColor = FLinearColor(0.78f, 0.78f, 0.8f, 1.0f);

	// --- Инвентарь: тексты ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Texts", meta = (DisplayPriority = "1"))
	FString InvHeaderText = TEXT("ИНВЕНТАРЬ  (Tab / I — закрыть)");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Texts", meta = (DisplayPriority = "2"))
	FString InvEquipmentHeaderText = TEXT("СНАРЯЖЕНИЕ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Texts", meta = (DisplayPriority = "3"))
	FString InvBackpackHeaderText = TEXT("РЮКЗАК");

	// Подписи слотов брони (строка «Голова: (пусто)» собирается кодом: имя + ": " + предмет).
	// ADR-048: (пусто)/Оружие/(нет)/Защита/использовать/надеть/Монеты/Голод/Жажда
	// переехали в UInventoryScreenWidget; здесь остались Canvas-специфичные.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Texts", meta = (DisplayPriority = "4"))
	FString InvSlotNameHead = TEXT("Голова");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Texts", meta = (DisplayPriority = "5"))
	FString InvSlotNameTorso = TEXT("Торс");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Texts", meta = (DisplayPriority = "6"))
	FString InvSlotNameLegs = TEXT("Штаны");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Texts", meta = (DisplayPriority = "11"))
	FString InvUnequipHintText = TEXT("(клик по занятому слоту — снять броню)");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Inventory Texts", meta = (DisplayPriority = "14"))
	FString InvDropButtonText = TEXT("X");

	// --- Диалог со старостой: геометрия и цвета (тексты реплик живут на AElderNPC) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "1"))
	float DialogPanelMaxWidth = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "2", ClampMin = "0.3", ClampMax = "1.0"))
	float DialogPanelScreenFrac = 0.86f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "3"))
	float DialogPadding = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "4"))
	float DialogButtonHeight = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "5"))
	float DialogButtonWidth = 200.0f;

	// Кнопка [Сдать (+N)] шире обычной — в ней сумма награды.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "6"))
	float DialogTurnInButtonWidth = 240.0f;

	// Отступ панели от низа экрана.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "7"))
	float DialogBottomMargin = 40.0f;

	// Пол высоты панели (короткие реплики) и потолок долей экрана (длинные).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "8"))
	float DialogMinPanelHeight = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "9", ClampMin = "0.2", ClampMax = "0.9"))
	float DialogMaxHeightFrac = 0.55f;

	// Цвет имени NPC в шапке диалога (голубой) и цвет текста реплики.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "10"))
	FLinearColor DialogNameColor = FLinearColor(0.65f, 0.88f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog", meta = (DisplayPriority = "11"))
	FLinearColor DialogTextColor = FLinearColor::White;

	// --- Диалог: тексты кнопок (Canvas-путь; префиксы кнопки сдачи переехали
	// в UDialogScreenWidget — ADR-048) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog Texts", meta = (DisplayPriority = "1"))
	FString DialogAcceptText = TEXT("[ Принять ]");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog Texts", meta = (DisplayPriority = "2"))
	FString DialogDeclineText = TEXT("[ Отказаться ]");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Dialog Texts", meta = (DisplayPriority = "3"))
	FString DialogCloseText = TEXT("[ Закрыть ]");

	// --- Экран смерти: геометрия/цвета (дополнение к базовым цветам выше) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "1"))
	float DeathTitleScale = 2.4f;

	// Вертикальные позиции долями экрана: заголовок и начало статистики.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "2", ClampMin = "0.0", ClampMax = "1.0"))
	float DeathTitleYFrac = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "3", ClampMin = "0.0", ClampMax = "1.0"))
	float DeathStatsYFrac = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "4"))
	float DeathStatScale = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "5"))
	float DeathPenaltyScale = 1.15f;

	// Цвет строки штрафа монет (оранжевый) и строки «сохранено» (зелёный).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "6"))
	FLinearColor DeathPenaltyColor = FLinearColor(0.95f, 0.55f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "7"))
	FLinearColor DeathSavedColor = FLinearColor(0.45f, 0.85f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "8"))
	float DeathButtonMaxWidth = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "9"))
	float DeathButtonHeight = 56.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "10"))
	float DeathButtonTextScale = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "11"))
	FLinearColor DeathButtonHoverColor = FLinearColor(0.3f, 0.6f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Layout", meta = (DisplayPriority = "12"))
	FLinearColor DeathKeyHintColor = FLinearColor(0.8f, 0.8f, 0.8f, 1.0f);

	// --- Экран смерти: тексты (штрафные строки — ФИНАЛЬНЫЕ формулировки ADR-027/ADR-044) ---

	// (Префиксы строк статистики и штрафа монет переехали в UDeathScreenWidget — ADR-048;
	// здесь остались Canvas-специфичные статичные строки.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Texts", meta = (DisplayPriority = "1"))
	FString DeathTitleText = TEXT("ВЫ ПОГИБЛИ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Texts", meta = (DisplayPriority = "7"))
	FString DeathRespawnLine = TEXT("Возрождение у костра в деревне.");

	// ADR-044 п.3 (дословно, дополнение Рината): БЕЗ «их можно забрать» — портит атмосферу.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Texts", meta = (DisplayPriority = "10"))
	FString DeathConsumablesLine = TEXT("Расходники обронены мешком на месте гибели.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Texts", meta = (DisplayPriority = "11"))
	FString DeathSavedLine = TEXT("Снаряжение, оружие и важные предметы сохранены.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Texts", meta = (DisplayPriority = "12"))
	FString DeathRespawnButtonText = TEXT("ВОЗРОДИТЬСЯ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death Texts", meta = (DisplayPriority = "13"))
	FString DeathKeyHintText = TEXT("Enter / Пробел — возродиться");

	// --- Трекер квеста (Canvas-путь; «Квест:»/«Квест выполнен…» переехали
	// в UQuestTrackerWidget — ADR-048; метка «Сдать:» — мировая, остаётся) ---

	// Перед названием на метке квестодателя: «Сдать: Шкуры волков».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Quest Texts", meta = (DisplayPriority = "4"))
	FString QuestTurnInMarkerPrefix = TEXT("Сдать: ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Quest Texts", meta = (DisplayPriority = "5"))
	FLinearColor QuestTrackerPlateColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.55f);

	// --- HUD игрока: цвет денег (Canvas-путь; префиксы HP/Hunger/Thirst/Ammo/Монеты
	// переехали в UPlayerStatsWidget — ADR-048) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Player Texts", meta = (DisplayPriority = "6"))
	FLinearColor PlayerMoneyColor = FLinearColor(1.0f, 0.85f, 0.2f, 1.0f);

	// --- Экран смерти (#26) ---

	// Затемнение фона экрана смерти (почти чёрное — фокус на статистике).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death", meta = (DisplayPriority = "1"))
	FLinearColor DeathDimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	// Цвет заголовка «Вы погибли».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death", meta = (DisplayPriority = "1"))
	FLinearColor DeathTitleColor = FLinearColor(0.9f, 0.12f, 0.1f, 1.0f);

	// Цвет строк статистики.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death", meta = (DisplayPriority = "1"))
	FLinearColor DeathStatColor = FLinearColor(0.95f, 0.95f, 0.95f, 1.0f);

	// Цвет кнопки «Возродиться».
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HUD|Death", meta = (DisplayPriority = "1"))
	FLinearColor DeathButtonColor = FLinearColor(0.2f, 0.45f, 0.25f, 1.0f);

private:
	// Рисует одну полоску здоровья над целью ЛЮБОГО типа (бандит/волк/любой враг
	// с UStatsComponent). Тип-агностично: принимает актёра и его компонент статов.
	// bIsCurrentTarget — текущая залоченная цель (рисуется ярким TargetFillColor).
	void DrawTargetHealthBar(AActor* TargetActor, UStatsComponent* Stats, bool bIsCurrentTarget);

	// Рисует маркер-ретикл (угловые скобки + указатель) над ТЕКУЩЕЙ залоченной целью,
	// чтобы игрок чётко видел активную цель (ФИКС1). Через DrawHUD, без UMG.
	void DrawTargetMarker(AActor* TargetActor);

	// Рисует статы игрока (HP-бар слева вверху, голод/жажда под ним, деньги-плашка) +
	// патроны экипированного дальнобоя (#5). Берёт Stats и CurrentWeapon у игрока.
	void DrawPlayerStats(APlayerCharacter* Player);

	// Рисует текст с тенью и обводкой (#18, читаемость на любом фоне) через FCanvasTextItem.
	// ScaleXY > 1 увеличивает кегль (шрифты HUD растровые, увеличение слегка мылит — ок для плашек).
	void DrawShadowedText(const FString& Text, const FLinearColor& Color, float X, float Y,
		class UFont* Font, float ScaleXY = 1.0f);

	// #18: текст с тёмной подложкой-плашкой под ним (плашка по размеру текста*scale + паддинг),
	// затем DrawShadowedText поверх. Для ключевых надписей магазина/инвентаря (деньги/цены).
	void DrawLabelWithPlate(const FString& Text, const FLinearColor& Color, float X, float Y,
		class UFont* Font, float ScaleXY = 1.0f);

	// Этап F: многострочный текст с переносом ПО СЛОВАМ в пределах MaxWidth (px). Нужен диалогу
	// старосты: длинная реплика (крючок кв.3) не влезает в одну строку панели, а DrawShadowedText
	// переносов не делает. Возвращает Y ПОСЛЕ последней отрисованной строки.
	float DrawWrappedText(const FString& Text, const FLinearColor& Color, float X, float Y,
		class UFont* Font, float MaxWidth, float ScaleXY = 1.0f);

	// Разбивает Text на строки по словам под MaxWidth (px) — ОБЩАЯ логика измерения и отрисовки
	// (фикс фидбека Рината 07-12: панель диалога сперва считает высоту реплики по числу строк,
	// затем рисует ТЕ ЖЕ строки — размер панели и текст не могут разойтись). Возвращает шаг
	// строки (px) по фактической метрике шрифта; при пустом тексте/шрифте OutLines пуст, шаг 0.
	float WrapTextIntoLines(const FString& Text, class UFont* Font, float MaxWidth, float ScaleXY,
		TArray<FString>& OutLines);

	// #18: обводка прямоугольника (4 линии) — рамка-акцент вокруг модальных панелей.
	void DrawRectOutline(float X, float Y, float W, float H, const FLinearColor& Color, float Thickness);

	// --- Маркеры интерактивных NPC / врагов / целей квеста (находимость) ---

	// Проходит по актёрам с IInteractableNPCInterface и рисует над каждым маркер
	// (в кадре — ромб + подпись; за кадром — стрелка по краю экрана к нему).
	void DrawInteractiveNPCMarkers();

	// Рисует маркер по мировому якорю: ромб+подпись в кадре либо краевую стрелку за кадром.
	// Color — цвет маркера (Этап D: NPC зелёный, враг-стрелок красный, цель квеста золотая).
	// bEdgeArrowOnly — рисовать ТОЛЬКО краевую стрелку за кадром (в кадре ничего): режим
	// стрелка-за-кадром (у видимого врага и так есть хелсбар).
	void DrawNPCMarker(const FVector& WorldAnchor, const FString& Label, const FLinearColor& Color,
		bool bEdgeArrowOnly = false);

	// Ромб-иконка + подпись по экранной точке (цель в кадре).
	void DrawNPCIcon(const FVector2D& ScreenPos, const FString& Label, const FLinearColor& Color);

	// Краевая стрелка, указывающая в сторону цели за пределами экрана.
	void DrawNPCEdgeArrow(const FVector2D& EdgePos, const FVector2D& Dir, const FLinearColor& Color);

	// D6 (ADR-035): красные краевые стрелки на врагов-СТРЕЛКОВ, ведущих бой за кадром
	// (реюз механизма краевых стрелок NPC — критерий D6).
	void DrawOffscreenShooterArrows();

	// Этап D: метка цели активного квеста (FQuest::MapMarkerTag). Пока цели НЕ выполнены (Active) —
	// на актор-цель (QuestMarkerTag базы AMasterEnemyBase или стандартный Actor Tag); квест готов
	// к сдаче (Completed) — на квестодателя (старосту), фидбек Рината 07-05. TurnedIn — гаснет.
	// В кадре — золотой ромб с подписью, за кадром — краевая стрелка.
	void DrawQuestTargetMarker(APlayerCharacter* Player);

	// Кэш актора-цели метки квеста (qa-фикс: НЕ перебирать все акторы мира каждый кадр).
	// Инвалидация: смена тега/фазы (цель <-> квестодатель) или гибель актора; неудачный поиск
	// повторяется не чаще чем раз в полсекунды (QuestMarkerNextSearchTime).
	TWeakObjectPtr<AActor> QuestMarkerTargetCache;
	FName QuestMarkerCachedTag = NAME_None;
	bool bQuestMarkerCachedToGiver = false;
	float QuestMarkerNextSearchTime = 0.0f;

	// Рисует контекстную подсказку взаимодействия («E — подобрать» / «E — торговать»)
	// по центру снизу (BUG3). Текст берётся у контроллера (ближайший интерактив).
	void DrawInteractPrompt(const FString& Text);

	// --- Всплывающие цифры урона (D5) ---

	// Одна живая цифра урона (не UObject — простая запись, живёт DamageNumberLifetime сек).
	struct FDamageNumberEntry
	{
		FVector WorldLocation = FVector::ZeroVector;
		float Amount = 0.0f;
		float SpawnTime = 0.0f;
	};

	// Живые цифры (чистятся по возрасту в DrawDamageNumbers).
	TArray<FDamageNumberEntry> DamageNumbers;

	// Рисует и старит цифры урона: подъём вверх + плавное затухание.
	void DrawDamageNumbers();

	// --- Экран инвентаря (immediate-mode) ---

	// Открыт ли экран инвентаря (модальный поверх HUD).
	bool bInventoryOpen = false;

	// UMG-экземпляр инвентаря (ADR-048): как ShopWidgetInstance — создаётся при первом
	// открытии, переиспользуется; пока на экране — Canvas-путь инвентаря заглушен.
	UPROPERTY()
	TObjectPtr<UInventoryScreenWidget> InventoryWidgetInstance;

	bool IsUmgInventoryActive() const;

	// Кликабельные зоны, пересобираемые каждый DrawInventory. Используются HandleInventoryClick.
	TArray<FInvHitRegion> InvHitRegions;

	// Рисует весь экран инвентаря: слева paper-doll (Head/Torso/Legs + оружие),
	// справа рюкзак (неэкипированные предметы), сверху деньги/голод/жажда. Заполняет InvHitRegions.
	void DrawInventory(APlayerCharacter* Player);

	// Рисует прямоугольную «плитку» (фон + опц. подсветка под курсором) и текст. Хелпер layout.
	void DrawInvBox(float X, float Y, float W, float H, const FLinearColor& BaseColor,
		const FVector2D& MousePos, const FString& Label, class UFont* Font);

	// Разрешает мягкую ссылку на иконку в текстуру С КЭШЕМ: одна попытка LoadSynchronous на
	// путь за жизнь HUD (nullptr-результат тоже кэшируется). Canvas рисует каждый кадр —
	// без кэша отсутствующая текстура (их ещё не нарисовал художник) грузилась бы и спамила
	// лог ежекадрово. Загруженные текстуры держит IconCache (UPROPERTY -> защита от GC).
	class UTexture2D* ResolveIcon(const TSoftObjectPtr<class UTexture2D>& SoftIcon);

	// Кэш иконок: путь -> текстура (nullptr = грузили, не нашли — больше не пытаемся).
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<UTexture2D>> IconCache;

	// Точка внутри прямоугольника зоны?
	static bool PointInRegion(const FVector2D& P, const FInvHitRegion& R);

	// --- Экран магазина (immediate-mode) ---

	bool bShopOpen = false;

	// UMG-экземпляр магазина (ADR-048): создаётся при первом открытии, если назначен
	// ShopWidgetClass; переиспользуется. Пока существует и на экране — Canvas-путь
	// магазина (DrawShop/клики/жесты) заглушен.
	UPROPERTY()
	TObjectPtr<UShopScreenWidget> ShopWidgetInstance;

	// Магазин живёт в UMG-пути? (слот назначен и экземпляр показан)
	bool IsUmgShopActive() const;

	// Вендор, чей каталог отрисовываем (источник цен/товаров). Интерфейс — развязка от
	// конкретного класса торговца (A2). TScriptInterface держит и UObject, и интерфейс-указатель.
	UPROPERTY()
	TScriptInterface<IShopVendor> ShopTrader;

	// Кликабельные зоны магазина, пересобираются каждый DrawShop.
	TArray<FShopHitRegion> ShopHitRegions;

	// Прокрутка списка «FOR SALE»: индекс первой видимой позиции каталога. Сбрасывается
	// при открытии/закрытии магазина, потолок (ShopListMaxScroll) пересчитывается каждый
	// DrawShop от фактической высоты панели.
	int32 ShopListScrollOffset = 0;
	int32 ShopListMaxScroll = 0;

	// Прокрутка правой колонки «SELL FROM BACKPACK» (G2): рюкзак тоже перерастает панель.
	int32 ShopSellScrollOffset = 0;
	int32 ShopSellMaxScroll = 0;

	// Прямоугольники колонок списков (пересобираются каждый DrawShop) — зоны тач-жестов.
	FVector2D ShopBuyAreaMin = FVector2D::ZeroVector;
	FVector2D ShopBuyAreaMax = FVector2D::ZeroVector;
	FVector2D ShopSellAreaMin = FVector2D::ZeroVector;
	FVector2D ShopSellAreaMax = FVector2D::ZeroVector;

	// Накопители пикселей свайпа (по зонам) и шаг строки последнего DrawShop: жест отдаёт
	// дробные дельты каждый кадр, строка листается при накоплении полного шага.
	float ShopScrollAccumBuy = 0.0f;
	float ShopScrollAccumSell = 0.0f;
	float ShopRowStep = 40.0f;

	// Рисует экран магазина: слева каталог (товары+цены+[buy]), справа рюкзак (предметы+[sell]),
	// сверху деньги + [Close]. Заполняет ShopHitRegions.
	void DrawShop(APlayerCharacter* Player);

	// --- Слайдер количества купли-продажи (Фаза 5, STALKER 2-стиль) ---

	// Активна ли транзакция со слайдером (поверх списков магазина).
	bool bSliderActive = false;

	// true = покупка позиции каталога (SliderEntryIndex); false = продажа предмета (SliderItem).
	bool bSliderIsBuy = false;

	// Индекс позиции каталога для покупки (-1 если продажа).
	int32 SliderEntryIndex = -1;

	// Продаваемый предмет (стак патронов) при продаже.
	UPROPERTY()
	AMasterInventoryItem* SliderItem = nullptr;

	// Выбранное количество и его потолок (по деньгам/размеру стака).
	int32 SliderQty = 1;
	int32 SliderQtyMax = 1;

	// Цена за ЕДИНИЦУ слайдера (для покупки — Price позиции; для продажи патрона — выкуп/патрон).
	float SliderUnitPrice = 0.0f;

	// Сколько патронов даёт одна единица покупки (для подписи «= N патронов»); 0 для не-патронов.
	int32 SliderUnitAmmo = 0;

	// Заголовок транзакции (имя товара/предмета).
	FString SliderTitle;

	// Армировать слайдер покупки/продажи (вызывается из HandleShopClick по клику Buy/Sell).
	void ArmBuySlider(APlayerCharacter* Player, int32 EntryIndex);
	void ArmSellSlider(APlayerCharacter* Player, AMasterInventoryItem* Item);

	// Закрыть слайдер (Cancel/после Confirm).
	void CancelShopSlider();

	// Нарисовать панель слайдера (трек+ручка, живая цена, кнопки) поверх списков. Добавляет
	// слайдер-зоны в ShopHitRegions. Возвращает геометрию трека через члены ниже (для hit-теста).
	void DrawShopSlider(APlayerCharacter* Player, const FVector2D& Mouse, UFont* Font,
		float SX, float SY);

	// Геометрия трека слайдера (пересобирается в DrawShopSlider) — для клика по треку.
	FVector2D SliderTrackMin = FVector2D::ZeroVector;
	FVector2D SliderTrackMax = FVector2D::ZeroVector;

	// --- Экран диалога со старостой (immediate-mode) ---

	bool bDialogOpen = false;

	// Староста, с которым идёт диалог (источник предлагаемого квеста).
	UPROPERTY()
	AElderNPC* DialogElder = nullptr;

	// UMG-экземпляр диалога (ADR-048): как ShopWidgetInstance.
	UPROPERTY()
	TObjectPtr<UDialogScreenWidget> DialogWidgetInstance;

	bool IsUmgDialogActive() const;

	// Кликабельные зоны диалога, пересобираются каждый DrawDialog.
	TArray<FDialogHitRegion> DialogHitRegions;

	// Рисует окно диалога: текст NPC (зависит от состояния квеста) + кнопки-ответы.
	// Поток: NotStarted -> [Принять]/[Отказаться]; Active -> прогресс + [Закрыть];
	// Completed -> [Сдать]; TurnedIn -> благодарность + [Закрыть]. Заполняет DialogHitRegions.
	void DrawDialog(APlayerCharacter* Player);

	// Рисует трекер активного квеста («Волков: X/5») в углу HUD, когда квест Active/Completed.
	void DrawQuestTracker(UQuestComponent* QuestComp);

	// --- Экран смерти (#26) ---

	// Открыт ли экран смерти (модальный поверх всего, кроме QA-оверлея).
	bool bDeathScreen = false;

	// UMG-экземпляры (ADR-048): экран смерти — по открытию; постоянные панели —
	// в BeginPlay. Пока экземпляр на экране — его Canvas-путь заглушен.
	UPROPERTY()
	TObjectPtr<UDeathScreenWidget> DeathWidgetInstance;

	UPROPERTY()
	TObjectPtr<UPlayerStatsWidget> PlayerStatsWidgetInstance;

	UPROPERTY()
	TObjectPtr<UQuestTrackerWidget> QuestTrackerWidgetInstance;

	UPROPERTY()
	TObjectPtr<UInteractPromptWidget> InteractPromptWidgetInstance;

	bool IsUmgDeathActive() const;

	// Прямоугольник кнопки «Возродиться» (пересобирается каждый DrawDeathScreen) — для hit-теста.
	FVector2D DeathRespawnBtnMin = FVector2D::ZeroVector;
	FVector2D DeathRespawnBtnMax = FVector2D::ZeroVector;

	// Рисует экран смерти: затемнение, заголовок «Вы погибли», статистика последней жизни
	// (прожил / от кого / деньги / квесты / убито) + кнопка «Возродиться» и подсказка-клавиша.
	void DrawDeathScreen(APlayerCharacter* Player);

	// --- QA-оверлей (Фаза 5, debug под автотестера) ---
	// Рисует кольцевой буфер последних QA-сообщений (FQADebug) в ПРАВОМ НИЖНЕМ углу:
	// жирный КРАСНЫЙ текст с тенью+обводкой (DrawShadowedText). Видимость — FQADebug::bOverlayVisible
	// (тумблер клавиша O; авто-вкл вместе с god-mode J). Рисуется последним — поверх всего.
	void DrawQADebugOverlay();

	// Цвет строк QA-оверлея (ярко-красный для заметности на любом фоне).
	UPROPERTY(EditDefaultsOnly, Category = "HUD|QA")
	FLinearColor QAOverlayColor = FLinearColor(1.0f, 0.12f, 0.1f, 1.0f);

	// Масштаб шрифта строк оверлея (растровый шрифт; >1 слегка мылит — допустимо для debug).
	UPROPERTY(EditDefaultsOnly, Category = "HUD|QA")
	float QAOverlayTextScale = 1.15f;
};
