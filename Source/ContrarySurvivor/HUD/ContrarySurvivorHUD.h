// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ContrarySurvivorHUD.generated.h"

class UStatsComponent;
class UQuestComponent;
class APlayerCharacter;
class AMasterInventoryItem;
class ATraderNPC;
class AElderNPC;

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

	// --- Маркер интерактивных NPC (торговец, позже староста) — находимость ---
	// Отличается от маркера ВРАГА (жёлтый ретикл): иной цвет (зелёный) и форма (ромб),
	// + стрелка по краю экрана, если NPC за кадром.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|NPCMarker")
	FLinearColor NPCMarkerColor = FLinearColor(0.15f, 0.95f, 0.45f, 1.0f); // зелёный (дружественный)

	// Полуразмер ромба маркера на экране (px), когда NPC в кадре.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|NPCMarker")
	float NPCMarkerHalfSize = 16.0f;

	// Толщина линий маркера/стрелки (px).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|NPCMarker")
	float NPCMarkerThickness = 3.0f;

	// Отступ от края экрана для зажатой к краю стрелки (px).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|NPCMarker")
	float NPCMarkerEdgeMargin = 56.0f;

	// Длина (px) указывающей стрелки за кадром.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|NPCMarker")
	float NPCMarkerArrowLen = 22.0f;

	// --- HUD игрока (GDD §7.7) ---

	// Левый верхний угол: отступы и размеры HP-бара игрока.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	float PlayerHudMarginX = 24.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	float PlayerHudMarginY = 24.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	float PlayerHealthBarWidth = 320.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	float PlayerHealthBarHeight = 28.0f;

	// Высота баров голода/жажды (#18: крупнее для читаемости).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	float PlayerSurvivalBarHeight = 24.0f;

	// Подложка-плашка под текстом денег (#18) — тёмный полупрозрачный прямоугольник.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	FLinearColor MoneyPlateColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	// Цвет текста патронов экипированного оружия (#5).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Player")
	FLinearColor AmmoColor = FLinearColor(0.95f, 0.95f, 0.95f, 1.0f);

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

	// --- Трекер активного квеста (Фаза 5, GDD §7.7) ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Quest")
	FLinearColor QuestTrackerColor = FLinearColor(1.0f, 0.85f, 0.3f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Quest")
	FLinearColor QuestTrackerDoneColor = FLinearColor(0.4f, 1.0f, 0.4f, 1.0f);

	// --- Экран смерти (#26) ---

	// Затемнение фона экрана смерти (почти чёрное — фокус на статистике).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Death")
	FLinearColor DeathDimColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	// Цвет заголовка «Вы погибли».
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Death")
	FLinearColor DeathTitleColor = FLinearColor(0.9f, 0.12f, 0.1f, 1.0f);

	// Цвет строк статистики.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Death")
	FLinearColor DeathStatColor = FLinearColor(0.95f, 0.95f, 0.95f, 1.0f);

	// Цвет кнопки «Возродиться».
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|Death")
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

	// --- Маркеры интерактивных NPC (находимость) ---

	// Проходит по актёрам с IInteractableNPCInterface и рисует над каждым маркер
	// (в кадре — ромб + подпись; за кадром — стрелка по краю экрана к нему).
	void DrawInteractiveNPCMarkers();

	// Рисует маркер одного NPC по мировому якорю: ромб+подпись в кадре либо краевую стрелку.
	void DrawNPCMarker(const FVector& WorldAnchor, const FString& Label);

	// Ромб-иконка + подпись по экранной точке (NPC в кадре).
	void DrawNPCIcon(const FVector2D& ScreenPos, const FString& Label);

	// Краевая стрелка, указывающая в сторону NPC за пределами экрана.
	void DrawNPCEdgeArrow(const FVector2D& EdgePos, const FVector2D& Dir);

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

	// --- Экран диалога со старостой (immediate-mode) ---

	bool bDialogOpen = false;

	// Староста, с которым идёт диалог (источник предлагаемого квеста).
	UPROPERTY()
	AElderNPC* DialogElder = nullptr;

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
