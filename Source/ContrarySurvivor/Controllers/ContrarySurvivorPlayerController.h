// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h"
#include "ContrarySurvivor/Actors/ShopVendor.h" // IShopVendor (ближайший вендор/магазин развязан от класса, A2)
#include "ContrarySurvivorPlayerController.generated.h"

class UStatsComponent;
class AElderNPC;
class APickup;

// Тип ближайшего контекстного интерактива (клавиша E, Фаза 4 — решение Рината/game-lead):
// E выбирает БЛИЖАЙШИЙ интерактив. Пикап -> подобрать, торговец -> магазин, староста -> диалог.
enum class EInteractKind : uint8
{
	None,
	Pickup,
	Trader,
	Elder
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

	// Регистрирует/сбрасывает ближайшего вендора (вызывает торговец-актёр через IShopVendor при overlap).
	void SetNearbyTrader(TScriptInterface<IShopVendor> Trader);
	void ClearNearbyTrader(TScriptInterface<IShopVendor> Trader);

	// Закрыть магазин (кнопка Close в UI / уход от торговца): вернуть режим ввода в Game.
	UFUNCTION(BlueprintCallable, Category = "Shop")
	void CloseShop();

	// --- Староста/диалог (Фаза 5) — вызываются старостой (overlap) и HUD (кнопки диалога) ---

	// Регистрирует/сбрасывает ближайшего старосту (вызывает AElderNPC при overlap).
	void SetNearbyElder(AElderNPC* Elder);
	void ClearNearbyElder(AElderNPC* Elder);

	// Закрыть диалог (кнопка [Закрыть]/[Отказаться] в UI / уход от старосты).
	UFUNCTION(BlueprintCallable, Category = "Dialog")
	void CloseDialog();

	// Закрыть ВСЕ открытые модальные окна (инвентарь/магазин/диалог) и вернуть режим ввода
	// в Game. Вызывается при смерти игрока (APlayerCharacter::HandleDeath), чтобы UI не
	// «зависал» открытым после респауна.
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseAllUI();

	// --- Экран смерти (#26) — вызываются APlayerCharacter (смерть/респаун) ---

	// Показать экран смерти: HUD-флаг + режим ввода UI (геймплей-ввод подавлен флагом bDeathScreen).
	UFUNCTION(BlueprintCallable, Category = "Death")
	void ShowDeathScreen();

	// Скрыть экран смерти и вернуть геймплейный режим ввода (зовётся в конце APlayerCharacter::Respawn).
	UFUNCTION(BlueprintCallable, Category = "Death")
	void HideDeathScreen();

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

	// Открывает магазин конкретного вендора (вынесено из OnInteract): HUD + режим ввода UI.
	void OpenShop(TScriptInterface<IShopVendor> Trader);

	// Открывает диалог с конкретным старостой (Фаза 5): предлагает квест журналу игрока
	// (OfferQuest), включает HUD-окно диалога и режим ввода UI.
	void OpenDialog(AElderNPC* Elder);

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

	// --- QA-харнесс (Фаза 5): дублёры квестов/диалога клавишами (тестер не кликает HUD/`~`) ---
	// Свободные буквенные клавиши (НЕ F5/F8/F11). Биндятся через legacy ActionMapping.

	// Y: телепорт игрока вплотную к ближайшему старосте (как T к торговцу).
	UFUNCTION()
	void OnQATeleportToElder();

	// G: предложить+принять квест у ближайшего старосты (= открыть диалог и нажать [Принять]).
	UFUNCTION()
	void OnQAAcceptQuest();

	// H: сдать выполненный квест ближайшему старосте (= [Сдать]).
	UFUNCTION()
	void OnQATurnInQuest();

	// K: зачесть одно убийство волка в квест (прогресс +1) без поиска живого волка.
	UFUNCTION()
	void OnQACreditWolfKill();

	// C: выдать игроку 5 «Шкур волка» в рюкзак (тест сдачи кв.1 без фарма волков).
	UFUNCTION()
	void OnQAGiveWolfHides();

	// X: выдать игроку «Ноутбук» в рюкзак (тест сдачи кв.2).
	UFUNCTION()
	void OnQAGiveNotebook();

	// Общий хелпер C/X: спавнит Count квест-предметов (AQuestItem) с заданным ItemName и кладёт в рюкзак.
	void GiveQuestItems(const FString& ItemName, int32 Count);

	// --- QA debug-инструменты (Фаза 5): god/forcedrop/spawn-wolf/overlay на клавишах J/U/B/O ---
	// Глобальные флаги в FQADebug, читаются в точках урона/деградации/дропа.

	// J: тумблер god-mode (неуязвимость игрока + заморозка убыли голода/жажды). Авто-вкл оверлей.
	UFUNCTION()
	void OnQAToggleGodMode();

	// U: тумблер force-drop (все враги роняют лут со 100% шансом).
	UFUNCTION()
	void OnQAToggleForceDrop();

	// B: заспавнить одного тест-волка рядом с игроком (быстро проверить лут).
	UFUNCTION()
	void OnQASpawnTestWolf();

	// O: тумблер показа экранного QA-оверлея.
	UFUNCTION()
	void OnQAToggleOverlay();

	// N: мгновенно убить БЛИЖАЙШЕГО врага (волк/бандит) штатным путём урона (TakeDamage),
	// чтобы сработали смерть + дроп лута + квест-счётчик. С активным force-drop (U) выпадет
	// предмет — тестер проверяет цепочку лута без прицеливания.
	UFUNCTION()
	void OnQAForceKillNearest();

	// Возрождение по клавише (Enter / Пробел) на экране смерти (#26): дубль кнопки «Возродиться»
	// (CU-мышь по HUD ненадёжна). Если экран смерти открыт — запускает APlayerCharacter::Respawn.
	UFUNCTION()
	void OnRespawnPressed();

	// P (QA, #26): мгновенно убить ИГРОКА штатным летальным уроном (для теста экрана смерти).
	// Учитывает god-mode (J): при god-mode не убивает (лог-skip).
	UFUNCTION()
	void OnQAKillPlayer();

	// Слайдер количества в магазине (Фаза 5): ±количество. Стрелки/колесо = ±1, с Shift = ±10.
	// Действуют только когда открыт магазин и активен слайдер транзакции.
	UFUNCTION()
	void OnShopQtyDec();
	UFUNCTION()
	void OnShopQtyInc();

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

	// QA: дебаунс тумблера god-mode (J). Отчёт тестера «GODMODE сам выключился сразу после
	// включения» = вероятный двойной IE_Pressed (CU/повтор клавиши). Игнорируем повторный
	// тоггл, пришедший в течение GodModeToggleDebounce секунд после предыдущего.
	double LastGodModeToggleTime = -1000.0;
	static constexpr double GodModeToggleDebounce = 0.30;

	// BugReport12 Этап1 (ФИКС движения): постоянный горизонтальный yaw для базиса WASD при
	// ФИКСИРОВАННОЙ изометрической камере. Дефолт 90 = совпадает с CameraBoomRotation.Yaw игрока
	// (экранное «вверх» сохраняется). Forward/right строятся от него в плоскости Z=0, а НЕ от
	// GetCameraRotation() — убирает зависимость от состояния камеры. Тюнингуется, если экранная
	// ориентация движения не совпадёт с камерой.
	UPROPERTY(EditAnywhere, Category = "Movement", meta = (AllowPrivateAccess = "true"))
	float MovementBasisYaw = 90.0f;

	// Открыт ли экран инвентаря (модальный): пока true — клик уходит в инвентарь (не стрельба),
	// движение подавлено. Зеркалит состояние HUD; источник переключения — OnToggleInventory.
	bool bInventoryOpen = false;

	// Открыт ли экран магазина (модальный, как инвентарь): клик уходит в магазин,
	// движение подавлено. Источник переключения — OnInteract/CloseShop.
	bool bShopOpen = false;

	// Ближайший вендор (выставляется его overlap-триггером). Пусто — торговца рядом нет.
	// Интерфейс — развязка от конкретного класса торговца (A2). TScriptInterface держит и
	// UObject (для IsValid/GetActorLocation через GetObject), и интерфейс-указатель (каталог/цены).
	UPROPERTY()
	TScriptInterface<IShopVendor> NearbyTrader;

	// Открыт ли экран диалога (модальный, как магазин): клик уходит в диалог, движение подавлено.
	bool bDialogOpen = false;

	// Открыт ли экран смерти (#26): геймплей-ввод (движение/огонь/интеракт) подавлен,
	// клик уходит в кнопку «Возродиться». Источник — ShowDeathScreen/HideDeathScreen.
	bool bDeathScreen = false;

	// Ближайший староста (выставляется его overlap-триггером). null — старосты рядом нет.
	UPROPERTY()
	AElderNPC* NearbyElder = nullptr;

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
