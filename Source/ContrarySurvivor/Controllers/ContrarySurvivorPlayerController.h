// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h"
#include "ContrarySurvivorPlayerController.generated.h"

class UStatsComponent;
class ATraderNPC;
class APickup;

// Тип ближайшего контекстного интерактива (клавиша E, Фаза 4 — решение Рината/game-lead):
// E выбирает БЛИЖАЙШИЙ интерактив. Пикап -> подобрать, торговец -> открыть магазин.
enum class EInteractKind : uint8
{
	None,
	Pickup,
	Trader
};

UCLASS()
class CONTRARYSURVIVOR_API AContrarySurvivorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AContrarySurvivorPlayerController();

	// Текущая захваченная цель (для HUD/индикатора). Публичный — читается из HUD.
	UFUNCTION(BlueprintPure, Category = "Combat")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	// --- Торговец/магазин (Фаза 4) — вызываются торговцем (overlap) и HUD (кнопка Close) ---

	// Регистрирует/сбрасывает ближайшего торговца (вызывает ATraderNPC при overlap).
	void SetNearbyTrader(ATraderNPC* Trader);
	void ClearNearbyTrader(ATraderNPC* Trader);

	// Закрыть магазин (кнопка Close в UI / уход от торговца): вернуть режим ввода в Game.
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void CloseShop();

	// --- Контекстная подсказка взаимодействия (E) — для HUD ---

	// Есть ли рядом интерактив (пикап/торговец), по которому E что-то сделает.
	bool HasInteractPrompt() const;

	// Текст подсказки («E — подобрать» / «E — торговать») для отрисовки на HUD.
	FString GetInteractPromptText() const;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// Каждый кадр поддерживает авто-лок на ближайшей живой цели (см. UpdateAutoTarget).
	virtual void Tick(float DeltaTime) override;

	// Радиус авто-захвата ближайшей живой цели (Unreal units). DRAFT — тюнингуется.
	// Авто-режим (вариант A, решение Рината): КАЖДЫЙ тик лочим БЛИЖАЙШУЮ живую цель в этом
	// радиусе и динамически перекидываем лок, если появилась ближе. Ручной фокус (тап по
	// врагу) выключает авто-переброс до смерти цели/тапа по другому/тапа по пустоте.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float AutoTargetRadius = 3000.0f;

	// --- Input Mapping ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	// --- Input Actions ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* InteractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* InventoryAction;

	// Клик/тап — выбор цели или выстрел
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* FireAction;

	// Перезарядка
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* ReloadAction;

	// --- Движение ---

	void Move(const FInputActionValue& Value);

	UFUNCTION()
	void Sprint(const FInputActionValue& Value);

	// --- Действия ---

	void Interact(const FInputActionValue& Value);
	void Inventory(const FInputActionValue& Value);

	// Нажатие кнопки атаки
	void Fire(const FInputActionValue& Value);

	// Перезарядка
	void Reload(const FInputActionValue& Value);

	// Переключение оружия (пистолет<->нож). Привязано через LEGACY ActionMapping
	// (Config/DefaultInput.ini), чтобы не плодить Enhanced Input .uasset без редактора (Фаза 3).
	UFUNCTION()
	void OnSwitchWeapon();

	// Открыть/закрыть экран инвентаря (клавиша Tab / I). Привязано через LEGACY ActionMapping
	// "ToggleInventory" (Config/DefaultInput.ini) — без нового IA/.uasset (Фаза 4).
	// Тогглит AContrarySurvivorHUD::ToggleInventory + переключает режим ввода (UI/Game).
	UFUNCTION()
	void OnToggleInventory();

	// Взаимодействие (LEGACY ActionMapping "Interact", клавиша E). Контекстно: если открыт
	// магазин — закрыть; иначе действовать по ближайшему интерактиву (пикап -> подобрать,
	// торговец -> открыть магазин). Выбор ближайшего обновляется в Tick (UpdateNearbyInteractable).
	UFUNCTION()
	void OnInteract();

	// Открывает магазин конкретного торговца (вынесено из OnInteract): HUD + режим ввода UI.
	void OpenShop(ATraderNPC* Trader);

	// --- QA-харнесс (Фаза 4 раунд 2): тест-действия на функциональные клавиши ---
	// Привязаны через LEGACY ActionMapping (Config/DefaultInput.ini), без нового IA/.uasset.
	// Нужны автотестеру (Computer Use), который не может открыть `~`-консоль (русская раскладка).
	// Дублируют существующие exec-команды — оба пути остаются.

	// F1: свободная/детач debug-камера (console-exec "ToggleDebugCamera", UCheatManager 5.5).
	UFUNCTION()
	void OnToggleDebugCamera();

	// F2: наполнить рюкзак тестовыми предметами (= APlayerCharacter::GiveTestItems).
	UFUNCTION()
	void OnTestGiveItems();

	// F3: надеть дефолтную броню всех слотов (= APlayerCharacter::EquipTestArmor).
	UFUNCTION()
	void OnTestEquipArmor();

	// F4: снять броню всех слотов (= APlayerCharacter::UnequipTestArmor).
	UFUNCTION()
	void OnTestUnequipArmor();

	// M (бывш. F5, перевешено из-за конфликта с вьюмодом Shader Complexity): +TestMoneyGrant
	// денег (для теста покупки у торговца).
	UFUNCTION()
	void OnTestGiveMoney();

	// T: тест-телепорт игрока вплотную к ближайшему торговцу (ATraderNPC) в радиус его
	// InteractTrigger — чтобы сработал NearbyTrader и заработали F9/F10/E. Волки не дают
	// подойти к прилавку сверху, поэтому нужен телепорт для верификации купли/продажи.
	UFUNCTION()
	void OnQATeleportToTrader();

	// --- QA-харнесс (Фаза 4 раунд 3): дублёры UI-действий клавишами ---
	// Тестер (Computer Use) НЕ может кликать HUD в PIE (мышь захвачена), поэтому те же
	// действия, что выполняются кликом, продублированы клавишами + явный LogQA для верификации.
	// Клики оставлены как есть (их проверяет Ринат). Привязка — legacy ActionMapping (DefaultInput.ini).

	// F6: использовать ПЕРВЫЙ расходник рюкзака (= клик «использовать»).
	UFUNCTION()
	void OnQAUseFirstConsumable();

	// F7: выбросить ПЕРВЫЙ предмет рюкзака (= клик [X]).
	UFUNCTION()
	void OnQADropFirstItem();

	// F9: купить самый дешёвый товар у ближайшего торговца (иначе пропуск с логом).
	UFUNCTION()
	void OnQABuyCheapest();

	// F10: продать первый предмет рюкзака ближайшему торговцу (иначе пропуск с логом).
	UFUNCTION()
	void OnQASellFirstItem();

	// F12: очистить слот сейва 'ContrarySave' (UGameplayStatics::DeleteGameInSlot).
	UFUNCTION()
	void OnQAClearSave();

	// Сколько денег выдаёт F5 за нажатие (DRAFT, тюнингуется).
	UPROPERTY(EditAnywhere, Category = "QA")
	float TestMoneyGrant = 100.0f;

	// Сбрасывает флаг «UI-клик уже обработан» при отпускании кнопки огня (BUG1: edge-клик).
	void OnFireReleased(const FInputActionValue& Value);

	// Клик/тап по экрану — захват цели под курсором (ADR-017: клик-захват).
	// Если под курсором валидный враг — захватываем (lock). Иначе текущий lock сохраняется.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TrySelectTarget();

private:
	bool IsSprinting = false;

	// Открыт ли экран инвентаря (модальный): пока true — клик уходит в инвентарь (не стрельба),
	// движение подавлено. Зеркалит состояние HUD; источник переключения — OnToggleInventory.
	bool bInventoryOpen = false;

	// Открыт ли экран магазина (модальный, как инвентарь): клик уходит в магазин,
	// движение подавлено. Источник переключения — OnInteract/CloseShop.
	bool bShopOpen = false;

	// Ближайший торговец (выставляется его overlap-триггером). null — торговца рядом нет.
	UPROPERTY()
	ATraderNPC* NearbyTrader = nullptr;

	// --- Контекстный interact по E (BUG3) ---

	// Ближайший интерактив (пикап/торговец), пересчитывается в Tick. Действие по E — над ним.
	UPROPERTY()
	AActor* CurrentInteractActor = nullptr;

	EInteractKind CurrentInteractKind = EInteractKind::None;

	// Радиус (см), в котором пикап предлагается к подбору по E. Для торговца берётся его
	// собственный overlap-триггер (NearbyTrader). DRAFT-тюнинг.
	UPROPERTY(EditAnywhere, Category = "Interact")
	float InteractRange = 300.0f;

	// Пересчитывает ближайший интерактив (пикап/торговец) для подсказки и действия по E.
	void UpdateNearbyInteractable();

	// BUG1 (edge-клик UI): true пока зажат клик, по которому уже выполнено одно UI-действие.
	// Сбрасывается на отпускании огня (OnFireReleased) и при открытии/закрытии модальных экранов.
	bool bUIClickConsumed = false;

	// Текущая захваченная цель (авто-ближайшая или ручной фокус).
	UPROPERTY()
	AActor* CurrentTarget;

	// QA-харнесс: последняя залогированная цель — чтобы логировать СМЕНУ цели один раз
	// (а не каждый тик). Сравнивается с CurrentTarget в Tick.
	UPROPERTY()
	AActor* LastLoggedTarget = nullptr;

	// Ручной фокус (вариант A): true, если игрок ЯВНО выбрал цель тапом/кликом по врагу.
	// Пока true и цель жива — авто-переброс на ближайшую НЕ происходит. Сбрасывается при
	// смерти цели, тапе по пустому месту или тапе по другому враге (с переустановкой на него).
	bool bManualLock = false;

	// LineTrace под курсором мыши для выбора цели
	AActor* GetActorUnderCursor();

	// Валидна ли цель для захвата/огня (существует и жива).
	bool IsValidTarget(AActor* Target) const;

	// Возвращает UStatsComponent актёра, ТОЛЬКО если это валидная цель-враг:
	// не сам игрок и несёт UStatsComponent. ТИП-АГНОСТИЧНО (бандит/волк/любой Pawn
	// со StatsComponent) — определяем «врага» по наличию компонента, не по классу.
	UStatsComponent* GetTargetStats(AActor* Actor) const;

	// Ищет ближайшую ЖИВУЮ цель (Pawn с UStatsComponent, не игрок, не мёртв)
	// в пределах AutoTargetRadius. null, если никого.
	AActor* FindNearestLivingTarget() const;

	// Авто-лок (вариант A): если ручного фокуса нет — КАЖДЫЙ тик берём ближайшую живую цель
	// (динамический переброс). Смерть/невалидность текущей цели сбрасывает ручной фокус и
	// возвращает в авто. Живой ручной lock сохраняется (не перекидывается авто).
	void UpdateAutoTarget();
};
