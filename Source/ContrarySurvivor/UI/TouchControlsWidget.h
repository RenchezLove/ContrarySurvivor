// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContrarySurvivor/UI/TouchControlsTypes.h"
#include "TouchControlsWidget.generated.h"

class UCanvasPanel;
class UImage;
class UButton;
class UTextBlock;
class UTexture2D;
class UInputAction;
class UEnhancedInputLocalPlayerSubsystem;
class AContrarySurvivorPlayerController;

/**
 * Настройки тач-слоя. Значения живут UPROPERTY на контроллере (Ринат тюнит на контроллере),
 * сюда копируются при создании виджета. Стилевые поля действуют ТОЛЬКО для дерева, построенного
 * кодом; у WBP-дерева стиль целиком в дизайнере (см. класс-коммент). Функциональные поля
 * (StickDeadZone, bSprintToggle) действуют в обоих режимах.
 * Plain-структура: в reflection не участвует (reflected-часть — FTouchButtonSettings).
 */
struct FTouchControlsConfig
{
	float StickRadius = 110.0f;       // радиус подложки стика, px
	float StickThumbRadius = 45.0f;   // радиус «шляпки», px
	FVector2D StickMargin = FVector2D(160.0f, 160.0f); // отступ центра стика от левого-нижнего угла
	float StickDeadZone = 0.15f;      // мёртвая зона (доля радиуса), внутри неё ввод = 0
	float IdleOpacity = 0.5f;         // прозрачность слоя в покое
	float ActiveOpacity = 0.85f;      // прозрачность активного элемента (стик/кнопка под пальцем)

	// Цвета стика (настройка Рината 07-18; дефолты = прежние зашитые).
	FLinearColor StickBaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.25f);
	FLinearColor StickThumbColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.6f);

	// Кнопки (шаг 2). Угол привязки каждой задаёт код виджета (см. BuildButtons).
	FTouchButtonSettings FireButton;      // ОГОНЬ — правый-нижний угол
	FTouchButtonSettings ReloadButton;    // ПЕРЕЗАРЯДКА — правый-нижний (над огнём)
	FTouchButtonSettings InteractButton;  // ДЕЙСТВИЕ (E) — правый-нижний (левее огня)
	FTouchButtonSettings SprintButton;    // БЕГ — правый-нижний (диагональ от огня)
	FTouchButtonSettings WeaponButton;    // ОРУЖИЕ (смена) — правый-нижний (над перезарядкой)
	FTouchButtonSettings InventoryButton; // СУМКА — правый-верхний
	FTouchButtonSettings PauseButton;     // ПАУЗА — левый-верхний (малозаметная)

	// true: бег — переключатель (тап вкл/выкл, кнопка подсвечена); false: держать кнопку.
	bool bSprintToggle = true;
};

/**
 * Экранное тач-управление (этап G, GDD ч.9: «Android: виртуальные стики/кнопки», ADR-017).
 * Стик движения + кнопки огонь/перезарядка/действие/бег/оружие/сумка/пауза.
 *
 * ДВА РЕЖИМА ДЕРЕВА (ADR-048, команда Рината 07-18 «менять интерфейс мышкой»):
 *  - слот Touch Controls Widget Class на контроллере ПУСТ -> дерево строится кодом
 *    (WidgetTree в NativeOnInitialized + BuildButtons), стиль — из настроек контроллера;
 *  - в слоте WBP_TouchControls (родитель этот класс) -> кубики приходят из дизайнера по
 *    BindWidgetOptional-именам (StickBase/StickThumb — Image; FireButton/ReloadButton/
 *    InteractButton/SprintButton/WeaponButton/InventoryButton/PauseButton — Button; подписи
 *    FireText/... — Text внутри кнопок, код их не трогает). Раскладку/цвет/размер/шрифт
 *    Ринат правит мышкой с живым превью; код НЕ трогает стиль WBP-кубиков — только
 *    функциональное состояние (видимость боевой группы, ход «шляпки» через Render Translation,
 *    подсветка переключателя БЕГ). Недостающий кубик -> Warning в лог, остальное работает.
 *
 * Архитектура ввода (одинакова в обоих режимах): слой НЕ изобретает свой ввод — всё уходит
 * в СУЩЕСТВУЮЩИЕ пути:
 *  - стик и боевые кнопки каждый кадр ИНЖЕКТИРУЮТСЯ в Enhanced Input-экшены контроллера
 *    (InjectInputVectorForAction / InjectInputForAction, EnhancedInputSubsystemInterface.h:148,160)
 *    — тот же путь, что WASD/ЛКМ/Shift/R, обработчики игры не тронуты;
 *  - кнопки легаси-действий (ДЕЙСТВИЕ/СУМКА/ОРУЖИЕ/ПАУЗА) зовут публичные Touch*-входы
 *    контроллера, которые дёргают ТЕ ЖЕ обработчики, что клавиши E/Tab/Q/Esc.
 *
 * Мышь и тач — единый путь (ADR-017: «клик = имитация тапа»): Pointer-события на ПК приходят
 * от мыши, на Android — от пальца; тач-обработчики делегируют в те же функции.
 *
 * Хит-зоны — ТОЛЬКО подложка стика и кнопки (корневая канва SelfHitTestInvisible; жест стика
 * начинается только если точка нажатия внутри StickBase): тапы по остальному экрану проходят
 * в мир (выбор цели/клики Canvas-HUD через ScreenTap, ADR-017).
 *
 * Пока открыто модальное окно (инвентарь/магазин/диалог/смерть/пауза) БОЕВАЯ группа
 * (стик+огонь+перезарядка+действие+бег+оружие) прячется и её инжекция глушится — иначе
 * нажатие «ОГОНЬ» при открытом инвентаре превратилось бы в клик по инвентарю в точке кнопки.
 * СУМКА и ПАУЗА остаются видимыми (повторный тап СУМКИ закрывает инвентарь — на телефоне
 * другого способа нет; ПАУЗУ прячет целиком контроллер через SetLayerEnabled).
 */
UCLASS()
class CONTRARYSURVIVOR_API UTouchControlsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Передаёт настройки и экшены контроллера (protected-поля — передаёт сам контроллер).
	// Звать сразу после CreateWidget, до AddToViewport.
	void InitTouch(AContrarySurvivorPlayerController* InController,
		const UInputAction* InMoveAction, const UInputAction* InFireAction,
		const UInputAction* InReloadAction, const UInputAction* InSprintAction,
		const FTouchControlsConfig& InConfig);

	// Прячет/показывает слой ЦЕЛИКОМ (контроллер зовёт при открытии/закрытии меню паузы).
	// Выключение сбрасывает зажатый стик и кнопки; Collapsed останавливает и NativeTick (инжекцию).
	void SetLayerEnabled(bool bEnabled);

	// --- Иконка текущего оружия (запрос Рината 07-19: «игрок не понимает какое оружие
	// в руках»). Мягкие ссылки — паттерн иконок брони HUD: текстуры может не быть,
	// тогда иконка не показывается, без крашей. Правится в Class Defaults WBP. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls|Weapon Icon", meta = (DisplayPriority = "1"))
	TSoftObjectPtr<UTexture2D> PistolIconTexture =
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Icons/T_Icon_Pistol.T_Icon_Pistol")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls|Weapon Icon", meta = (DisplayPriority = "2"))
	TSoftObjectPtr<UTexture2D> KnifeIconTexture =
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/UI/Icons/T_Icon_Knife.T_Icon_Knife")));

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

	// --- Обработчики кнопок (AddDynamic требует UFUNCTION) ---

	UFUNCTION() void HandleFirePressed();
	UFUNCTION() void HandleFireReleased();
	UFUNCTION() void HandleReloadPressed();
	UFUNCTION() void HandleSprintPressed();
	UFUNCTION() void HandleSprintReleased();
	UFUNCTION() void HandleInteractPressed();
	UFUNCTION() void HandleWeaponPressed();
	UFUNCTION() void HandleInventoryPressed();
	UFUNCTION() void HandlePausePressed();

	// --- Кубики WBP_TouchControls (имена ТОЧНЫЕ — см. umg-layout-guide.md).
	// При дереве из кода эти же поля заполняет BuildButtons — вся логика ниже общая. ---

	// Подложка стика (хит-зона жеста; центр и радиус стика в WBP-режиме берутся из её геометрии).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> StickBase;

	// «Шляпка» стика; в WBP-режиме ходит за пальцем через Render Translation.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> StickThumb;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> FireButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ReloadButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> InteractButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SprintButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> WeaponButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> InventoryButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PauseButton;

	// Иконка текущего оружия (пистолет/нож). Текстуру и видимость ставит КОД по
	// экипированному оружию (UpdateWeaponIcon); позицию/размер Ринат двигает в дизайнере.
	// Кубика нет в WBP — виджет создаёт иконку сам (fallback, позиция кодовая).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> WeaponIconImage;

	// Подписи кнопок (Text внутри соответствующей кнопки). В WBP-режиме код их НЕ трогает
	// (текст/шрифт/цвет — Рината в дизайнере), привязка — задел под будущие динамические
	// подписи (например патроны на ПЕРЕЗАРЯДЕ). В кодовом режиме создаёт MakeTouchButton.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FireText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReloadText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InteractText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SprintText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WeaponText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InventoryText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PauseText;

private:
	// Угол экрана, к которому прижата кнопка (Margin отсчитывается от него).
	enum class ETouchCorner : uint8
	{
		BottomRight,
		TopRight,
		TopLeft
	};

	// Общая логика для мыши и тача (единый путь ADR-017).
	FReply HandlePointerDown(const FGeometry& Geo, const FPointerEvent& Ev);
	FReply HandlePointerMove(const FGeometry& Geo, const FPointerEvent& Ev);
	FReply HandlePointerUp(const FGeometry& Geo, const FPointerEvent& Ev);

	// Центр стика в локальных координатах виджета: кодовое дерево — низ-лево + StickMargin,
	// WBP — центр StickBase из его отрисованной геометрии.
	FVector2D GetStickCenterLocal(const FGeometry& Geo) const;

	// Радиус стика, px: кодовое дерево — Config.StickRadius, WBP — полширины StickBase.
	float GetStickRadiusPx() const;

	// Пересчитывает вектор стика из позиции указателя и двигает «шляпку».
	void UpdateStickFromPointer(const FGeometry& Geo, const FPointerEvent& Ev);

	// Возвращает «шляпку» в центр, вектор в ноль.
	void ResetStick();

	// Строит кнопки по конфигу — только для кодового дерева (зовётся из InitTouch).
	void BuildButtons();

	// Круглая кнопка с подписью, прижатая к углу Corner; добавляет в RootCanvas. null, если
	// S.bEnabled=false. Подпись/цвета/шрифт — из S (EditAnywhere-настройки контроллера);
	// созданный Text подписи кладётся в OutLabel (тот же член, что биндится из WBP).
	UButton* MakeTouchButton(const FTouchButtonSettings& S, ETouchCorner Corner,
		const FName& WidgetName, TObjectPtr<UTextBlock>& OutLabel);

	// Подписка обработчиков на непустые кнопки (общая для обоих режимов; зовётся из InitTouch).
	void BindButtonHandlers();

	// Сбор боевой группы (стик + огонь/перезаряд/действие/бег/оружие) с запоминанием
	// «показанной» видимости каждого кубика — для восстановления после модалок.
	void CollectCombatGroup();

	// Показ/скрытие боевой группы (модальное окно открыто -> прячем) + сброс зажатий.
	void SetCombatGroupVisible(bool bVisible);

	// Сброс зажатых кнопок (огонь/бег) с восстановлением визуала.
	void ResetHeldButtons();

	// --- Иконка текущего оружия ---

	// Состояние иконки (обновление ТОЛЬКО на смене — без загрузок/SetBrush каждый кадр).
	enum class EWeaponIconState : uint8 { Unknown, NoWeapon, Pistol, Knife };

	// Создаёт кубик WeaponIconImage кодом в канву Canvas (кодовое дерево или fallback
	// для WBP без кубика): 48x48 над кнопкой ОРУЖИЕ (позиция из Config).
	void CreateWeaponIconInCanvas(UCanvasPanel* Canvas);

	// Сверяет оружие пешки контроллера с показанным и применяет смену (текстура+видимость).
	// bForceHide: модальное окно открыто — иконка прячется вместе с боевой группой.
	void UpdateWeaponIcon(bool bForceHide);

	// Сабсистема Enhanced Input локального игрока (null, если игрока нет).
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;

	// Корень кодового дерева (в WBP-режиме null — корень там строит Ринат).
	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	// Боевая группа: корневые виджеты элементов, прячущихся при модальном окне.
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> CombatGroupWidgets;

	// Видимость каждого кубика боевой группы в «показанном» состоянии (параллелен
	// CombatGroupWidgets): у WBP-кубиков восстанавливаем выставленное Ринатом, не жёсткое Visible.
	TArray<ESlateVisibility> CombatGroupShownVisibility;

	UPROPERTY()
	TObjectPtr<AContrarySurvivorPlayerController> OwnerPC;

	// Экшены контроллера (те же, что у клавиатуры/мыши) — цели инжекции.
	UPROPERTY()
	TObjectPtr<const UInputAction> MoveActionRef;

	UPROPERTY()
	TObjectPtr<const UInputAction> FireActionRef;

	UPROPERTY()
	TObjectPtr<const UInputAction> ReloadActionRef;

	UPROPERTY()
	TObjectPtr<const UInputAction> SprintActionRef;

	FTouchControlsConfig Config;

	// true = дерево пришло из WBP (Ринат), false = построено кодом. Ставится в NativeOnInitialized.
	bool bDesignerTree = false;

	// Цвет кнопки БЕГ в покое: белый у кодового дерева, у WBP — снятый с кнопки Рината
	// (переключатель подсвечивается SprintActiveTint и возвращается к этому цвету).
	FLinearColor SprintIdleColor = FLinearColor::White;

	bool bStickActive = false;
	int32 StickPointerIndex = INDEX_NONE;              // какой палец/кнопка держит стик
	FVector2D StickVector = FVector2D::ZeroVector;     // нормализованный вектор [-1..1] (X вправо, Y вперёд)

	// Кэш загруженных текстур иконки (LoadSynchronous один раз на смену оружия).
	UPROPERTY()
	TObjectPtr<UTexture2D> ResolvedPistolIcon;

	UPROPERTY()
	TObjectPtr<UTexture2D> ResolvedKnifeIcon;

	EWeaponIconState WeaponIconState = EWeaponIconState::Unknown;

	bool bFireHeld = false;      // кнопка ОГОНЬ зажата -> инжекция IA_Fire каждый кадр (автоогонь)
	bool bSprintOn = false;      // бег активен (переключатель или удержание) -> инжекция IA_Sprint
	bool bReloadQueued = false;  // одноразовая инжекция IA_Reload на следующем кадре
	bool bCombatGroupVisible = true; // текущее состояние боевой группы (чтобы не дёргать каждый кадр)
};
