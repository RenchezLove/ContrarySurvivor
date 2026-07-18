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
class UInputAction;
class UEnhancedInputLocalPlayerSubsystem;
class AContrarySurvivorPlayerController;

/**
 * Настройки тач-слоя. Значения живут UPROPERTY на контроллере (виджет строится кодом и в
 * Details не виден — Ринат тюнит на контроллере), сюда копируются при создании виджета.
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
 * Шаг 1: виртуальный СТИК ДВИЖЕНИЯ слева. Шаг 2: кнопки огонь/перезарядка/действие/бег/
 * оружие/сумка/пауза (полный состав по требованию Рината для проверки сборки на телефоне).
 *
 * Архитектура: слой НЕ изобретает свой ввод — всё уходит в СУЩЕСТВУЮЩИЕ пути:
 *  - стик и боевые кнопки каждый кадр ИНЖЕКТИРУЮТСЯ в Enhanced Input-экшены контроллера
 *    (InjectInputVectorForAction / InjectInputForAction, EnhancedInputSubsystemInterface.h:148,160)
 *    — тот же путь, что WASD/ЛКМ/Shift/R, обработчики игры не тронуты;
 *  - кнопки легаси-действий (ДЕЙСТВИЕ/СУМКА/ОРУЖИЕ/ПАУЗА) зовут публичные Touch*-входы
 *    контроллера, которые дёргают ТЕ ЖЕ обработчики, что клавиши E/Tab/Q/Esc.
 *
 * Мышь и тач — единый путь (ADR-017: «клик = имитация тапа»): Pointer-события на ПК приходят
 * от мыши, на Android — от пальца; тач-обработчики делегируют в те же функции.
 *
 * Хит-зоны — ТОЛЬКО подложка стика и кнопки (корневая канва SelfHitTestInvisible): тапы по
 * остальному экрану проходят в мир (выбор цели/клики Canvas-HUD через ScreenTap, ADR-017).
 *
 * Пока открыто модальное окно (инвентарь/магазин/диалог/смерть/пауза) БОЕВАЯ группа
 * (стик+огонь+перезарядка+действие+бег+оружие) прячется и её инжекция глушится — иначе
 * нажатие «ОГОНЬ» при открытом инвентаре превратилось бы в клик по инвентарю в точке кнопки.
 * СУМКА и ПАУЗА остаются видимыми (повторный тап СУМКИ закрывает инвентарь — на телефоне
 * другого способа нет; ПАУЗУ прячет целиком контроллер через SetLayerEnabled).
 *
 * Дерево целиком строится в C++ (WidgetTree), BP-наследник не нужен — паттерн окон этапа F.
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

	// Центр стика в локальных координатах виджета (низ-лево + отступ).
	FVector2D GetStickCenterLocal(const FGeometry& Geo) const;

	// Пересчитывает вектор стика из позиции указателя и двигает «шляпку».
	void UpdateStickFromPointer(const FGeometry& Geo, const FPointerEvent& Ev);

	// Возвращает «шляпку» в центр, вектор в ноль.
	void ResetStick();

	// Строит кнопки по конфигу (зовётся из InitTouch — настройки уже известны).
	void BuildButtons();

	// Круглая кнопка с подписью, прижатая к углу Corner; добавляет в RootCanvas и в боевую
	// группу (bCombatGroup). null, если S.bEnabled=false. Подпись/цвета/шрифт — из S
	// (EditAnywhere-настройки контроллера, дефолты подписей задаёт его конструктор).
	UButton* MakeTouchButton(const FTouchButtonSettings& S, ETouchCorner Corner,
		const FName& WidgetName, bool bCombatGroup);

	// Показ/скрытие боевой группы (модальное окно открыто -> прячем) + сброс зажатий.
	void SetCombatGroupVisible(bool bVisible);

	// Сброс зажатых кнопок (огонь/бег) с восстановлением визуала.
	void ResetHeldButtons();

	// Сабсистема Enhanced Input локального игрока (null, если игрока нет).
	UEnhancedInputLocalPlayerSubsystem* GetInputSubsystem() const;

	UPROPERTY()
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY()
	TObjectPtr<UImage> StickBase;

	UPROPERTY()
	TObjectPtr<UImage> StickThumb;

	// Кнопки, чьё состояние трогаем после создания (null, если выключены конфигом).
	UPROPERTY()
	TObjectPtr<UButton> FireButton;

	UPROPERTY()
	TObjectPtr<UButton> SprintButton;

	// Боевая группа: корневые виджеты элементов, прячущихся при модальном окне
	// (подложка+шляпка стика, огонь/перезарядка/действие/бег/оружие).
	UPROPERTY()
	TArray<TObjectPtr<UWidget>> CombatGroupWidgets;

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

	bool bStickActive = false;
	int32 StickPointerIndex = INDEX_NONE;              // какой палец/кнопка держит стик
	FVector2D StickVector = FVector2D::ZeroVector;     // нормализованный вектор [-1..1] (X вправо, Y вперёд)

	bool bFireHeld = false;      // кнопка ОГОНЬ зажата -> инжекция IA_Fire каждый кадр (автоогонь)
	bool bSprintOn = false;      // бег активен (переключатель или удержание) -> инжекция IA_Sprint
	bool bReloadQueued = false;  // одноразовая инжекция IA_Reload на следующем кадре
	bool bCombatGroupVisible = true; // текущее состояние боевой группы (чтобы не дёргать каждый кадр)
};
