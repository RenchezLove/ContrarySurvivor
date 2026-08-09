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
#include "ContrarySurvivor/UI/TouchControlsTypes.h" // FTouchButtonSettings (настройки тач-кнопок, этап G)
#include "ContrarySurvivor/UI/PauseMenuWidget.h"    // FPauseMenuStyle (стиль меню паузы — поле контроллера)
#include "ContrarySurvivor/UI/StartScreenWidget.h"  // FStartScreenStyle (Б3: стиль стартового экрана)
#include "ContrarySurvivor/Debug/QADebug.h"         // CONTRARY_WITH_QA_CHEATS: отладочных клавиш нет в Shipping
#include "ContrarySurvivorPlayerController.generated.h"

class UStatsComponent;
class AElderNPC;
class APickup;
class APlayerCharacter;                    // цель-деревня интро ищется от позиции игрока
class UOnboardingComponent;
class UTouchControlsWidget;
class UPauseMenuWidget;
class UStartScreenWidget;                  // Б3: экран «Продолжить»/«Новая игра», строится кодом
class USettingsScreenWidget;               // Подход 2 волны меню: экран настроек (ADR-062)
class UIntroScreenWidget;                 // Build 1: экран интро (чёрный + строки), строится кодом
enum class EShopDragZone : uint8; // зоны тач-жестов магазина (ContrarySurvivorHUD.h, G2)

// Фаза скриптового интро (Build 1, ТЗ раздел 2). Идёт по порядку; None — интро не играет.
enum class EIntroPhase : uint8
{
	None,       // интро не идёт (закончилось / после смерти / отключено)
	Line1,      // чёрный экран, первая строка проступает и гаснет
	Line2,      // чёрный экран, вторая строка проступает и гаснет
	Reveal,     // мир проявляется из черноты, персонаж сам идёт к деревне
	AutoWalk,   // мир виден, персонаж ещё идёт сам, затем передача управления
	HandOff     // управление у игрока; ведём грейд-арку и меняем задачу у околицы деревни
};

// Тип ближайшего контекстного интерактива (клавиша E, Фаза 4 — решение Рината/game-lead):
// E выбирает БЛИЖАЙШИЙ интерактив. Пикап -> подобрать, торговец -> магазин, староста -> диалог,
// труп врага с лутом -> окно обыска (Build 1.2.1, ТЗ А1).
enum class EInteractKind : uint8
{
	None,
	Pickup,
	Trader,
	Elder,
	Corpse
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

	// --- Обыск трупа (Build 1.2.1, ТЗ А1) — вызывается OnInteract (E у трупа) и HUD ---

	// Открыть окно обыска (контейнер лута — на трупе врага или на мешке-пикапе, Build 1.2.2).
	UFUNCTION(BlueprintCallable, Category = "CorpseLoot")
	void OpenCorpseLoot(class UCorpseLootComponent* Corpse);

	// Закрыть окно обыска (крестик / Esc / повторное E / труп исчез по таймеру).
	UFUNCTION(BlueprintCallable, Category = "CorpseLoot")
	void CloseCorpseLoot();

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

	// Переводимый текст подсказки целиком: «Обыскать — E» на компьютере, «Обыскать —
	// ДЕЙСТВИЕ» на телефоне. Показывает UMG-панель UInteractPromptWidget.
	FText GetInteractPromptDisplayText() const;

	// Тот же текст для СТАРОГО Canvas-пути рисования (AContrarySurvivorHUD::DrawInteractPrompt,
	// по ADR-048 выпиливается вместе с остальным Canvas-кодом). Новый код зовёт версию выше.
	FString GetInteractPromptText() const;

	// Текст ДЕЙСТВИЯ для вида интерактива, без способа выполнения («Обыскать»). Вид передаётся
	// параметром, чтобы подсказку можно было собрать и проверить без живой сцены (автотесты).
	// bPickupUsesSearchWindow: мешок-пикап с окном обыска подписывается как труп («Обыскать»),
	// мгновенный подбор остаётся «Подобрать».
	FText GetInteractActionText(EInteractKind Kind, bool bPickupUsesSearchWindow = false) const;

	// Текст СПОСОБА выполнить действие: имя клавиши на компьютере, подпись экранной кнопки
	// при показанном тач-слое.
	FText GetInteractHowText() const;

	// Способ по отдельности — публично, потому что автотесты собирают обе подсказки (для
	// компьютера и для телефона) из РЕАЛЬНЫХ настроек, а тач-слой headless не поднимается.
	FText GetInteractKeyName() const { return InteractPromptKeyName; }

	// Подпись экранной кнопки; пустое поле — берётся у самой кнопки, чтобы подсказка с ней
	// не разошлась (см. комментарий к полю InteractPromptTouchButtonName).
	FText GetInteractTouchButtonName() const
	{
		return InteractPromptTouchButtonName.IsEmpty() ? TouchInteractButton.Label : InteractPromptTouchButtonName;
	}

	FText GetInteractPromptFormat() const { return InteractPromptFormat; }

	// Склейка «действие — способ» по шаблону. Статическая и без состояния: та же математика
	// проверяется автотестами для обоих способов управления.
	static FText FormatInteractPrompt(const FText& Format, const FText& ActionText, const FText& HowText);

	// Чистое правило «Продолжить посреди интро-этапа»: баннер задачи и стрелку-указатель
	// восстанавливаем, только когда интро в принципе включено и журнал квестов после загрузки
	// пуст (до старосты игрок не дошёл; с непустым журналом ориентиры даёт трекер квестов).
	// Статическая и без состояния — покрыта автотестами (ContrarySurvivor.SaveLoad).
	static bool ShouldResumeIntroObjectiveAfterContinue(bool bIntroEnabled, int32 RestoredQuestCount);

	// --- Этап F: онбординг/окно ежедневки ---

	// Открыт ли какой-либо модальный экран (инвентарь/магазин/диалог/обыск трупа/экран
	// смерти/меню паузы/стартовый экран Б3). Нужно UDailyRewardComponent: возвращать GameOnly
	// после окна награды можно только если игрок не успел открыть другую модалку.
	bool IsAnyModalUIOpen() const { return bInventoryOpen || bShopOpen || bDialogOpen || bCorpseLootOpen || bDeathScreen || bPauseMenuOpen || bStartScreenOpen || bSettingsScreenOpen; }

	// Показано ли сейчас ГЛАВНОЕ МЕНЮ (само меню либо открытый поверх него экран настроек).
	// Дефект с телефона 08-09: поверх меню оставался игровой интерфейс — полосы здоровья,
	// голода, жажды и деньги. Постоянные панели HUD прячутся по этому признаку и возвращаются
	// сами, когда меню закрылось.
	bool IsMainMenuOnScreen() const { return bStartScreenOpen || bSettingsScreenOpen; }

	// Пока на экране главное меню, звуки ИГРОВОГО МИРА молчат (жалоба Рината 08-09: «в меню
	// играет Ambient локации, поют птицы»). Причина в том, что игра грузит сразу игровой
	// уровень (GameDefaultMap), а меню открывается поверх него. Глушим именно мир: звуки
	// самого интерфейса (щелчки кнопок, музыка меню) помечены как «звук интерфейса» и мы их
	// не трогаем. Метод ИДЕМПОТЕНТЕН и сам сравнивает нужное состояние с текущим — его можно
	// звать хоть каждый кадр, хоть по событию открытия и закрытия.
	void ApplyWorldAudioGate();

	// Чистое правило отбора (гоняется автотестом): глушим звук, только если он НЕ интерфейсный,
	// сейчас звучит и ещё не приостановлен кем-то другим. Последнее важно: чужую паузу мы не
	// снимаем — вернём к жизни ровно то, что усыпили сами.
	static bool ShouldSilenceWorldSound(bool bIsUISound, bool bIsPlaying, bool bAlreadyPaused);

	// Подавлен ли сейчас ввод движения интро-последовательностью (чёрный экран строк /
	// авто-подход к деревне). Нужно индикатору хромоты (Build 1): разовая расшифровка
	// «почему герой идёт медленно» показывается только когда управление уже у игрока.
	bool IsIntroMoveInputLocked() const { return bIntroInputLocked; }

	// --- Меню паузы (этап G, меню-минимум: пауза + «Продолжить» + «Выход») ---

	// Закрыть меню паузы и снять паузу мира (кнопка «Продолжить» / повторный Esc).
	UFUNCTION(BlueprintCallable, Category = "Pause")
	void ClosePauseMenu();

	// --- Входы тач-кнопок (этап G, шаг 2) — зовёт UTouchControlsWidget ---
	// Дёргают ТЕ ЖЕ обработчики, что клавиши легаси-привязок (E/Tab/Q/Esc): никаких новых
	// путей ввода, тач — тонкая обёртка поверх существующих (ADR-017).

	void TouchInteract()        { OnInteract(); }
	void TouchToggleInventory() { OnToggleInventory(); }
	void TouchSwitchWeapon()    { OnSwitchWeapon(); }
	void TouchTogglePauseMenu() { OnTogglePauseMenu(); }

	// Тач-слой активен (Android всегда; ПК — по флагу bEnableTouchControls). HUD выбирает
	// по этому текст подсказки прокрутки магазина: «свайп» против «колесо» (G2).
	bool HasTouchLayer() const { return TouchControlsLayer != nullptr; }

	// Чистое правило «показывать ли главное меню при этом запуске» (ADR-062, покрыто
	// автотестом ContrarySurvivor.MainMenu): меню открывается со второго запуска после
	// установки ЛИБО при найденном сейве (сейв сам доказывает прошлый запуск).
	static bool ShouldShowMainMenuOnLaunch(bool bLaunchedBefore, bool bHasSave);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// Каждый кадр поддерживает авто-лок на ближайшей живой цели (см. UpdateAutoTarget).
	virtual void Tick(float DeltaTime) override;

	// --- Главное меню (ADR-062; выросло из стартового экрана Б3) ---

	// Открывает главное меню и блокирует геймплей (SetPause, как меню паузы) до выбора
	// игрока. Показывается со второго запуска (решает MaybeStartIntro); наличие сейва
	// передаётся виджету (без сейва «Продолжить» не показывается вовсе). Интро НЕ запускается.
	void OpenStartScreen();

	// Закрывает стартовый экран и снимает паузу (вызывается ОБОИМИ обработчиками кнопок —
	// решение уже принято, дальше выбором распоряжается вызывающий).
	void CloseStartScreen();

	// «Продолжить»: закрыть экран, загрузить сейв целиком (APlayerCharacter::LoadGameForContinue).
	// Интро НЕ запускается (Б3, замечание 9 ревизии — повторный показ интро при каждом запуске).
	void HandleStartScreenContinue();

	// «Новая игра» поверх существующего сейва: закрыть экран, честно стереть сейв
	// (APlayerCharacter::ResetToNewGame) и запустить обычную новую игру (с полным интро).
	void HandleStartScreenNewGame();

	// «Выход» главного меню: закрыть игру (тот же путь, что «Выход» меню паузы).
	void HandleStartScreenExit();

	// «В главное меню» из паузы (подход 3 волны меню): закрыть паузу и открыть главное меню.
	// Подтверждение при несохранённом прогрессе спрашивает сам виджет паузы — сюда приходит
	// уже окончательное решение.
	void HandlePauseMainMenu();

	// --- Экран настроек (ADR-062, подход 2 волны меню) ---

	// «Настройки» главного меню: открыть экран настроек поверх меню. Сама привязка этого
	// обработчика ещё и ПОКАЗЫВАЕТ пункт «Настройки»: виджет меню держит пункт спрятанным,
	// пока владелец не подписался на OnSettingsRequested (UStartScreenWidget::ApplyMenuRowVisibility).
	void HandleStartScreenSettings();

	// Открыть/закрыть экран настроек (мир уже на паузе под главным меню — повторно не паузим).
	void OpenSettingsScreen();
	void CloseSettingsScreen();

	// Настройка изменилась: освежить то, что живёт в мире, — прозрачность и чувствительность
	// экранного управления и громкость уже играющего звука (сама настройка применяется и
	// сохраняется в UContrarySurvivorGameUserSettings).
	void ApplyPlayerSettingsToWorld();

	// Двойной переспрос сброса пройден: стереть сохранение и освежить главное меню
	// («Продолжить» обязан пропасть — сохранения больше нет).
	void HandleSettingsResetProgress();

	// --- Интро (Build 1, ТЗ раздел 2) ---

	// Один раз (когда пешка появилась) решает дальнейший ход (ADR-062): не первый запуск на
	// устройстве ЛИБО найден сейв — открыть главное меню, иначе (самый первый запуск после
	// установки) сразу StartNewGameFlow. Интро играет ТОЛЬКО при новой игре — прежнего
	// варианта «интро с hold-to-skip при повторном заходе» больше нет.
	void MaybeStartIntro();

	// Обычная новая игра: запускает полное (не пропускаемое) интро, если оно включено.
	// Вызывается и когда сейва не было вовсе, и после «Новая игра» на стартовом экране.
	void StartNewGameFlow();

	// Запускает интро: чёрный экран, блок ввода движения, тёмный грейд, цель-деревня.
	void StartIntro(bool bSkippable);

	// Кадровый шаг интро (тайминг строк, проявление мира, авто-подход, грейд-арка, смена задачи).
	void UpdateIntro(float DeltaTime);

	// Передать управление игроку: снять чёрный, разблокировать ввод, показать подсказку движения.
	void IntroHandOverControl();

	// Завершить интро полностью (у околицы деревни): сменить задачу на «найти старосту», стоп.
	void EndIntro();

	// «Продолжить» посреди интро-этапа (журнал квестов после загрузки пуст — до старосты игрок
	// ещё не дошёл): интро не переигрывается, но баннер задачи и стрелка-указатель обязаны
	// вернуться. Ставит задачу/стрелку на деревню и переводит интро-машину в фазу HandOff —
	// штатный переход «вошёл в деревню → найти старосту» (UpdateIntro/EndIntro) отработает сам,
	// в том числе если сейв уже внутри деревни (EndIntro сработает на первом же кадре).
	void ResumeIntroObjectiveAfterContinue();

	// Найти цель-деревню интро: актор с тегом VillageMarkerTag, фолбэк — ближайший староста.
	// Заполняет IntroVillageActor/IntroVillageLocation/bIntroHasVillage/IntroInitialDistance.
	void FindIntroVillageTarget(const APlayerCharacter* PlayerChar);

	// Держит ли игрок сейчас клавишу/палец пропуска (для hold-to-skip на повторных заходах).
	bool IsIntroSkipHeld() const;

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

	// --- Тач-управление (этап G, ADR-017: тач поверх той же абстракции ввода) ---
	// Все параметры тюнингуются на BP контроллера в редакторе БЕЗ перекомпиляции (директива
	// Рината): настройки живут здесь и копируются в виджет при создании (BeginPlay).

	// WBP тач-слоя (ADR-048, паттерн слотов «HUD|UMG Widgets»): пусто — слой строится кодом
	// как раньше (стиль из полей ниже); назначен WBP_TouchControls (родитель
	// TouchControlsWidget) — раскладку/цвет/размер/шрифт кнопок и стика Ринат правит мышкой
	// в дизайнере, стилевые поля ниже для него не действуют (функциональные — действуют).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls|UMG Widgets", meta = (DisplayPriority = 0))
	TSubclassOf<UTouchControlsWidget> TouchControlsWidgetClass;

	// Показ виртуального стика на ПК (для теста мышью: клик по стику = имитация пальца,
	// код-путь тот же). На Android слой включается всегда, флаг не нужен.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 1))
	bool bEnableTouchControls = false;

	// Радиус подложки виртуального стика, px.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 2, ClampMin = "20.0"))
	float TouchStickRadius = 110.0f;

	// Радиус «шляпки» стика (кружок под пальцем), px.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 3, ClampMin = "5.0"))
	float TouchStickThumbRadius = 45.0f;

	// Отступ ЦЕНТРА стика от левого-нижнего угла экрана, px (X вправо, Y вверх).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 4))
	FVector2D TouchStickMargin = FVector2D(160.0f, 160.0f);

	// Мёртвая зона стика (доля радиуса): отклонение меньше — движения нет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 5, ClampMin = "0.0", ClampMax = "0.9"))
	float TouchStickDeadZone = 0.15f;

	// Прозрачность слоя в покое (0 — невидим, 1 — непрозрачен).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 6, ClampMin = "0.0", ClampMax = "1.0"))
	float TouchIdleOpacity = 0.5f;

	// Прозрачность стика под пальцем.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 7, ClampMin = "0.0", ClampMax = "1.0"))
	float TouchActiveOpacity = 0.85f;

	// --- Тач-кнопки (шаг 2, полный состав по требованию Рината). Дефолты раскладки —
	// в конструкторе; угол привязки каждой кнопки задан кодом виджета (веер правого-нижнего
	// угла + СУМКА справа-сверху + ПАУЗА слева-сверху), Margin отсчитывается от этого угла. ---

	// ОГОНЬ: держать = автоогонь (инжекция IA_Fire каждый кадр, как зажатая ЛКМ).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 8))
	FTouchButtonSettings TouchFireButton;

	// ПЕРЕЗАРЯД: одноразовая инжекция IA_Reload (клавиша R).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 9))
	FTouchButtonSettings TouchReloadButton;

	// ДЕЙСТВИЕ: тот же обработчик, что клавиша E (подобрать/торговать/диалог/закрыть).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 10))
	FTouchButtonSettings TouchInteractButton;

	// БЕГ: переключатель или удержание — см. bTouchSprintToggle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 11))
	FTouchButtonSettings TouchSprintButton;

	// ОРУЖИЕ: смена пистолет<->нож, тот же обработчик, что клавиша Q.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 12))
	FTouchButtonSettings TouchWeaponButton;

	// СУМКА: открыть/закрыть инвентарь (Tab). Видна и при открытых окнах — повторный тап закрывает.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 13))
	FTouchButtonSettings TouchInventoryButton;

	// ПАУЗА: малозаметная кнопка меню паузы в углу (дубль системной «назад» Android).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 14))
	FTouchButtonSettings TouchPauseButton;

	// true: БЕГ — переключатель (тап вкл/выкл, подсветка); false: бег пока палец на кнопке.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 15))
	bool bTouchSprintToggle = true;

	// Цвета стика (были зашиты в виджете; белый полупрозрачный = прежний вид).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 18))
	FLinearColor TouchStickBaseColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls", meta = (DisplayPriority = 19))
	FLinearColor TouchStickThumbColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.6f);

	// Стиль меню паузы (цвета/тексты/шрифты/размер кнопок) — применяется при создании виджета
	// (директива Рината 07-18: настройка в BP контроллера без пересборки).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu", meta = (DisplayPriority = "1"))
	FPauseMenuStyle PauseMenuStyle;

	// Стиль стартового экрана «Продолжить»/«Новая игра» (Б3) — тот же паттерн настройки
	// без пересборки, что у меню паузы.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen", meta = (DisplayPriority = "1"))
	FStartScreenStyle StartScreenStyle;

	// --- Слоты WBP-классов окон контроллера (ТЗ Рината 08-07, архитектура ADR-048; паттерн
	// TouchControlsWidgetClass выше). Пусто — окно строится кодом, как раньше; назначен
	// WBP_* (родитель — соответствующий C++-класс) — вид окна правится мышкой в дизайнере.
	// Пустые слоты заполняет режим генератора -hudslots. ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pause Menu", meta = (DisplayPriority = "0"))
	TSubclassOf<UPauseMenuWidget> PauseMenuWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Start Screen", meta = (DisplayPriority = "0"))
	TSubclassOf<UStartScreenWidget> StartScreenWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings Screen", meta = (DisplayPriority = "0"))
	TSubclassOf<USettingsScreenWidget> SettingsScreenWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro", meta = (DisplayPriority = "0"))
	TSubclassOf<UIntroScreenWidget> IntroScreenWidgetClass;

	// Тексты контекстной подсказки взаимодействия (низ-центр экрана, рисует HUD).
	// Локализация (ADR-050): FText, дефолты через NSLOCTEXT (LOCTEXT в значении по
	// умолчанию UHT запрещает — UhtTextProperty.cs:104).
	//
	// ПОДСКАЗКА ВСЕГДА СОСТОИТ ИЗ ДВУХ ЧАСТЕЙ (задача Рината 08-06): сначала ДЕЙСТВИЕ
	// («Обыскать»), затем СПОСОБ его выполнить («E» на компьютере, «ДЕЙСТВИЕ» — имя
	// экранной кнопки — на телефоне). Склеивает их шаблон InteractPromptFormat, получается
	// «Обыскать — E» или «Обыскать — ДЕЙСТВИЕ». Раньше на телефоне печаталось одно голое
	// действие, и игрок не понимал, чем его выполнить.
	//
	// Какой способ назвать, решает HasTouchLayer: тот же признак, по которому HUD выбирает
	// подсказку прокрутки магазина. Признак отвечает «показан ли тач-слой», а не «телефон ли
	// это» — намеренно: подсказка обязана называть то управление, которое игрок видит.
	//
	// Блок ПУБЛИЧНЫЙ: автотесты подменяют текст действия и шаблон, проверяя, что подсказка
	// собирается из настроек, а не из зашитых строк.
public:

	// Шаблон склейки: {Action} — что произойдёт, {How} — чем это сделать.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact",
		meta = (DisplayName = "Подсказка: шаблон «действие — способ»", DisplayPriority = "1"))
	FText InteractPromptFormat = NSLOCTEXT("ContrarySurvivorPlayerController", "InteractPromptFormat", "{Action} — {How}");

	// Способ на компьютере — имя клавиши. Реальная привязка «Interact» = E
	// (Config/DefaultInput.ini; там же дубль на кнопку геймпада, её подсказка не называет).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact",
		meta = (DisplayName = "Подсказка: клавиша на компьютере", DisplayPriority = "2"))
	FText InteractPromptKeyName = NSLOCTEXT("ContrarySurvivorPlayerController", "InteractPromptKeyName", "E");

	// Способ на телефоне — подпись экранной кнопки. Должна совпадать с подписью самой кнопки
	// (TouchInteractButton.Label, по умолчанию «ДЕЙСТВИЕ»); оставить поле ПУСТЫМ — подсказка
	// возьмёт подпись прямо у кнопки и никогда с ней не разойдётся.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact",
		meta = (DisplayName = "Подсказка: название экранной кнопки (пусто — взять у кнопки)", DisplayPriority = "3"))
	FText InteractPromptTouchButtonName = NSLOCTEXT("ContrarySurvivorPlayerController", "InteractPromptTouchButtonName", "ДЕЙСТВИЕ");

	// --- Тексты самих действий (без способа; способ подставит шаблон выше) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact",
		meta = (DisplayName = "Действие: предмет на земле", DisplayPriority = "4"))
	FText InteractPromptPickupAction = NSLOCTEXT("ContrarySurvivorPlayerController", "InteractPromptPickupAction", "Подобрать");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact",
		meta = (DisplayName = "Действие: торговец", DisplayPriority = "5"))
	FText InteractPromptTraderAction = NSLOCTEXT("ContrarySurvivorPlayerController", "InteractPromptTraderAction", "Торговать");

	// Дословная формулировка Рината (08-06): у старосты подсказка называет собеседника.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact",
		meta = (DisplayName = "Действие: староста", DisplayPriority = "6"))
	FText InteractPromptElderAction = NSLOCTEXT("ContrarySurvivorPlayerController", "InteractPromptElderAction", "Поговорить со старостой");

	// Build 1.2.1 (ТЗ А1): труп врага с лутом. Build 1.2.2: тот же текст у мешка-пикапа,
	// который открывает окно обыска (действие одно и то же).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact",
		meta = (DisplayName = "Действие: труп или мешок с вещами", DisplayPriority = "7"))
	FText InteractPromptCorpseAction = NSLOCTEXT("ContrarySurvivorPlayerController", "InteractPromptCorpseAction", "Обыскать");

protected:

	// --- Интро (Build 1, ТЗ издателя раздел 2). Тексты дословно из ТЗ; тюнинг длительностей —
	// Ринату. Тексты видит игрок → FText/NSLOCTEXT (ADR-050). ---

	// Мастер-выключатель интро (удобство отладки команды: снять галку — интро не играет вовсе).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro", meta = (DisplayPriority = "0"))
	bool bEnableIntro = true;

	// Две короткие строки на чёрном экране (по очереди, каждая ~IntroLineDuration).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Texts", meta = (DisplayPriority = "1", MultiLine = "true"))
	FText IntroLine1 = NSLOCTEXT("Intro", "Line1", "Столица осталась позади. И всё, что в ней было.");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Texts", meta = (DisplayPriority = "2", MultiLine = "true"))
	FText IntroLine2 = NSLOCTEXT("Intro", "Line2", "Впереди — дым над крышами. Значит, там ещё живут.");

	// Задача вверху по центру после проявления мира: дойти до деревни.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Texts", meta = (DisplayPriority = "3"))
	FText IntroObjectiveGoToVillage = NSLOCTEXT("Intro", "ObjGoVillage", "Впереди деревня. Дойти до неё.");

	// Задача при входе в безопасную зону деревни: найти старосту.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Texts", meta = (DisplayPriority = "4"))
	FText IntroObjectiveFindElder = NSLOCTEXT("Intro", "ObjFindElder", "Найти старосту и поговорить.");

	// Подсказка пропуска (показывается только при повторных заходах — hold-to-skip).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Texts", meta = (DisplayPriority = "5"))
	FText IntroSkipHintText = NSLOCTEXT("Intro", "SkipHint", "Зажмите, чтобы пропустить");

	// Длительность показа каждой строки на чёрном, с.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro", meta = (ClampMin = "0.5", DisplayPriority = "1"))
	float IntroLineDuration = 3.0f;

	// Длительность плавного проявления мира из черноты, с.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro", meta = (ClampMin = "0.1", DisplayPriority = "2"))
	float IntroRevealDuration = 2.0f;

	// Сколько персонаж идёт сам к деревне (от начала проявления мира), с. 2-4 (ТЗ).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro", meta = (ClampMin = "0.0", DisplayPriority = "3"))
	float IntroAutoApproachDuration = 3.0f;

	// Радиус вокруг цели-деревни (см), вход в который считается «в деревне» → смена задачи.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro", meta = (ClampMin = "50.0", DisplayPriority = "4"))
	float IntroSafeZoneRadius = 1500.0f;

	// Сколько держать клавишу/палец, чтобы пропустить интро (только повторные заходы), с.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro", meta = (ClampMin = "0.2", DisplayPriority = "5"))
	float IntroSkipHoldTime = 0.8f;

	// Тег актора-центра деревни: к нему ведёт стрелка интро и от него считается «в деревне».
	// Пусто/не найден — фолбэк на ближайшего старосту (AElderNPC).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro", meta = (DisplayPriority = "6"))
	FName VillageMarkerTag = TEXT("VillageCenter");

	// --- Тач-жесты магазина (G2): свайп = прокрутка списков / количество слайдера ---

	// Порог (px), после которого касание считается свайпом, а не тапом. Меньше — прокрутка
	// отзывчивее, но дрожащий палец начнёт листать вместо кликов по кнопкам.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls|Shop", meta = (DisplayPriority = 16, ClampMin = "1.0"))
	float TouchDragSlopPx = 14.0f;

	// Множитель скорости свайп-прокрутки списков (1 = список движется за пальцем 1:1).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Touch Controls|Shop", meta = (DisplayPriority = 17, ClampMin = "0.1"))
	float TouchScrollSensitivity = 1.0f;

	// Обработчики BindTouch (SetupInputComponent). Активны только при открытом магазине:
	// отслеживается ОДИН палец (первый коснувшийся); свайп по списку — прокрутка, свайп по
	// треку слайдера — количество, отпускание без свайпа — клик (HandleShopClick) в точке
	// ОТПУСКАНИЯ. Клик магазина на НАЖАТИИ (прежний путь ScreenTap->Fire) для тача снят —
	// иначе каждый свайп начинался бы покупкой того, что под пальцем.
	void OnShopTouchPressed(ETouchIndex::Type FingerIndex, FVector Location);
	void OnShopTouchMoved(ETouchIndex::Type FingerIndex, FVector Location);
	void OnShopTouchReleased(ETouchIndex::Type FingerIndex, FVector Location);

	// Сброс отслеживания пальца (отпускание/закрытие магазина).
	void ResetShopTouchState();

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

	// Меню паузы (этап G): тумблер по легаси-привязке "PauseMenu" (Esc/L/Android Back,
	// Config/DefaultInput.ini). У привязки bExecuteWhenPaused=true — клавиша работает и при
	// паузе (ввод контроллера обрабатывается на пауза-тике, PlayerController.cpp:5133).
	UFUNCTION()
	void OnTogglePauseMenu();

	// Открывает меню паузы: виджет + SetPause(true) + режим ввода UI; тач-слой прячется.
	void OpenPauseMenu();

	// Кнопка «Выход» меню паузы: закрыть игру (UKismetSystemLibrary::QuitGame).
	void HandlePauseQuit();

	// Тап по экрану на Android (легаси-действие "ScreenTap" = Touch1..Touch3, DefaultInput.ini).
	// Тапы НЕ порождают клик мыши для игрового ввода (порождают клавиши Touch*,
	// PlayerInput.cpp:473), а IMC_Default маплит IA_Fire только на ЛКМ — поэтому тап заводится
	// в ТОТ ЖЕ Fire()-путь вручную: выбор цели/выстрел/клики Canvas-HUD (ADR-017: тап = клик).
	// Позиция тапа приходит через GetMousePosition: вьюпорт кеширует позицию пальца как
	// позицию курсора (SceneViewport.cpp:842). На ПК Touch*-клавиши не генерятся — путь мыши
	// не задет. Тапы, съеденные UMG (стик/кнопки/меню), сюда не доходят — дублей нет.
	UFUNCTION()
	void OnScreenTapPressed();
	UFUNCTION()
	void OnScreenTapReleased();

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

	// ======================================================================
	// ОТЛАДОЧНЫЕ КЛАВИШИ. Всё, что ниже до закрывающего #endif, В ПУБЛИКАЦИОННОЙ СБОРКЕ
	// НЕ СУЩЕСТВУЕТ: выключатель CONTRARY_WITH_QA_CHEATS (Debug/QADebug.h) в режиме Shipping
	// равен нулю, и эти объявления не компилируются вовсе (Б5 задания издателя).
	// Макроса UFUNCTION здесь намеренно НЕТ: привязка идёт обычным указателем на метод
	// (InputComponent->BindAction), отражение не требуется, а UFUNCTION внутри условной
	// компиляции сгенерировал бы код на несуществующие функции. Тем же приёмом закрыт
	// движковый UCheatManager.
	// ======================================================================
#if CONTRARY_WITH_QA_CHEATS

	// --- QA-харнесс (Фаза 4 раунд 2): тест-действия на функциональные клавиши ---
	// Привязаны через LEGACY ActionMapping (Config/DefaultInput.ini), без нового IA/.uasset.
	// Нужны автотестеру (Computer Use), который не может открыть `~`-консоль (русская раскладка).
	// Дублируют существующие exec-команды — оба пути остаются.

	// F1: свободная/детач debug-камера (console-exec "ToggleDebugCamera", UCheatManager 5.5).
	void OnToggleDebugCamera();

	// F2: наполнить рюкзак тестовыми предметами (= APlayerCharacter::GiveTestItems).
	void OnTestGiveItems();

	// F3: надеть тест-комплект брони, по умолчанию полный Т3 (= APlayerCharacter::EquipTestArmor).
	void OnTestEquipArmor();

	// F4: снять броню всех слотов (= APlayerCharacter::UnequipTestArmor).
	void OnTestUnequipArmor();

	// M (бывш. F5, перевешено из-за конфликта с вьюмодом Shader Complexity): +TestMoneyGrant
	// денег (для теста покупки у торговца).
	void OnTestGiveMoney();

	// T: тест-телепорт игрока вплотную к ближайшему торговцу (ATraderNPC) в радиус его
	// InteractTrigger — чтобы сработал NearbyTrader и заработали F9/F10/E. Волки не дают
	// подойти к прилавку сверху, поэтому нужен телепорт для верификации купли/продажи.
	void OnQATeleportToTrader();

	// --- QA-харнесс (Фаза 4 раунд 3): дублёры UI-действий клавишами ---
	// Тестер (Computer Use) НЕ может кликать HUD в PIE (мышь захвачена), поэтому те же
	// действия, что выполняются кликом, продублированы клавишами + явный LogQA для верификации.
	// Клики оставлены как есть (их проверяет Ринат). Привязка — legacy ActionMapping (DefaultInput.ini).

	// F6: использовать ПЕРВЫЙ расходник рюкзака (= клик «использовать»).
	void OnQAUseFirstConsumable();

	// F7: выбросить ПЕРВЫЙ предмет рюкзака (= клик [X]).
	void OnQADropFirstItem();

	// F9: купить самый дешёвый товар у ближайшего торговца (иначе пропуск с логом).
	void OnQABuyCheapest();

	// F10: продать первый предмет рюкзака ближайшему торговцу (иначе пропуск с логом).
	void OnQASellFirstItem();

	// F12: очистить слот сейва 'ContrarySave' (UGameplayStatics::DeleteGameInSlot).
	void OnQAClearSave();

	// --- QA-харнесс (Фаза 5): дублёры квестов/диалога клавишами (тестер не кликает HUD/`~`) ---
	// Свободные буквенные клавиши (НЕ F5/F8/F11). Биндятся через legacy ActionMapping.

	// Y: телепорт игрока вплотную к ближайшему старосте (как T к торговцу).
	void OnQATeleportToElder();

	// G: предложить+принять квест у ближайшего старосты (= открыть диалог и нажать [Принять]).
	void OnQAAcceptQuest();

	// H: сдать выполненный квест ближайшему старосте (= [Сдать]).
	void OnQATurnInQuest();

	// K: зачесть одно убийство волка в квест (прогресс +1) без поиска живого волка.
	void OnQACreditWolfKill();

	// C: выдать игроку 5 «Шкур волка» в рюкзак (тест сдачи кв.1 без фарма волков).
	void OnQAGiveWolfHides();

	// X: выдать игроку «Ноутбук» в рюкзак (тест сдачи кв.2).
	void OnQAGiveNotebook();

	// Общий хелпер C/X: спавнит Count квест-предметов (AQuestItem) с заданным ItemName и кладёт в рюкзак.
	void GiveQuestItems(const FString& ItemName, int32 Count);

	// --- QA debug-инструменты (Фаза 5): god/forcedrop/spawn-wolf/overlay на клавишах J/U/B/O ---
	// Глобальные флаги в FQADebug, читаются в точках урона/деградации/дропа.

	// J: тумблер god-mode (неуязвимость игрока + заморозка убыли голода/жажды). Авто-вкл оверлей.
	void OnQAToggleGodMode();

	// U: тумблер force-drop (все враги роняют лут со 100% шансом).
	void OnQAToggleForceDrop();

	// B: заспавнить одного тест-волка рядом с игроком (быстро проверить лут).
	void OnQASpawnTestWolf();

	// O: тумблер показа экранного QA-оверлея.
	void OnQAToggleOverlay();

	// N: мгновенно убить БЛИЖАЙШЕГО врага (волк/бандит) штатным путём урона (TakeDamage),
	// чтобы сработали смерть + дроп лута + квест-счётчик. С активным force-drop (U) выпадет
	// предмет — тестер проверяет цепочку лута без прицеливания.
	void OnQAForceKillNearest();

	// P (QA, #26): мгновенно убить ИГРОКА штатным летальным уроном (для теста экрана смерти).
	// Учитывает god-mode (J): при god-mode не убивает (лог-skip).
	void OnQAKillPlayer();

#endif // CONTRARY_WITH_QA_CHEATS

	// Возрождение по клавише (Enter / Пробел) на экране смерти (#26): дубль кнопки «Возродиться»
	// (CU-мышь по HUD ненадёжна). Если экран смерти открыт — запускает APlayerCharacter::Respawn.
	// ЭТО ИГРОВАЯ клавиша, а не отладочная — остаётся в публикационной сборке.
	UFUNCTION()
	void OnRespawnPressed();

	// Слайдер количества в магазине (Фаза 5): ±количество. Стрелки/колесо = ±1, с Shift = ±10.
	// Действуют только когда открыт магазин и активен слайдер транзакции.
	UFUNCTION()
	void OnShopQtyDec();
	UFUNCTION()
	void OnShopQtyInc();

	// Этап F (онбординг): ЛЮБОЙ ввод гасит активную подсказку. Биндится на EKeys::AnyKey с
	// bConsumeInput=false — ввод идёт дальше в игру, мы только подглядываем.
	void OnAnyInputForHints();

	// Компонент онбординга подконтрольного игрока (null до possess/не наш пешка).
	UOnboardingComponent* GetOnboarding() const;

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

	// --- Состояние тач-жеста магазина (G2, см. OnShopTouch*) ---

	// Индекс отслеживаемого пальца (ETouchIndex как int; INDEX_NONE — жеста нет).
	int32 ShopTouchFinger = INDEX_NONE;
	FVector2D ShopTouchStart = FVector2D::ZeroVector;
	FVector2D ShopTouchLast = FVector2D::ZeroVector;
	bool bShopTouchDragging = false;

	// Зона, где жест начался (фиксируется при превышении порога свайпа). Инициализируется
	// в конструкторе: enum объявлен forward, значения здесь недоступны.
	EShopDragZone ShopTouchZone;

	// Ближайший вендор (выставляется его overlap-триггером). Пусто — торговца рядом нет.
	// Интерфейс — развязка от конкретного класса торговца (A2). TScriptInterface держит и
	// UObject (для IsValid/GetActorLocation через GetObject), и интерфейс-указатель (каталог/цены).
	UPROPERTY()
	TScriptInterface<IShopVendor> NearbyTrader;

	// Открыт ли экран диалога (модальный, как магазин): клик уходит в диалог, движение подавлено.
	bool bDialogOpen = false;

	// Открыто ли окно обыска трупа (Build 1.2.1, ТЗ А1; модальное, как магазин): клики
	// обрабатывают Slate-кнопки окна, движение подавлено. Источник — OpenCorpseLoot/CloseCorpseLoot.
	bool bCorpseLootOpen = false;

	// Открыт ли экран смерти (#26): геймплей-ввод (движение/огонь/интеракт) подавлен,
	// клик уходит в кнопку «Возродиться». Источник — ShowDeathScreen/HideDeathScreen.
	bool bDeathScreen = false;

	// Открыто ли меню паузы (этап G): мир на SetPause, клики глушатся барьером виджета.
	// Источник переключения — OnTogglePauseMenu (Esc/L/Android Back) и кнопки меню.
	bool bPauseMenuOpen = false;

	// Открыт ли стартовый экран «Продолжить»/«Новая игра» (Б3): мир на SetPause, как меню паузы.
	// Источник переключения — MaybeStartIntro (открытие) и HandleStartScreen*/CloseStartScreen.
	bool bStartScreenOpen = false;

	// Открыт ли экран настроек (ADR-062, подход 2): показывается поверх главного меню,
	// мир к этому моменту уже на паузе. Источник — OpenSettingsScreen/CloseSettingsScreen.
	bool bSettingsScreenOpen = false;

	// Паузу мира поставил САМ экран настроек (а не меню под ним). Только в этом случае
	// закрытие настроек снимает паузу — иначе «Назад» оживил бы мир под открытым меню.
	bool bPausedBySettingsScreen = false;

	// Экранный тач-слой (этап G): создаётся в BeginPlay на Android или при bEnableTouchControls.
	// null — слой выключен.
	UPROPERTY()
	TObjectPtr<UTouchControlsWidget> TouchControlsLayer;

	// --- Заглушение мира на время главного меню (см. ApplyWorldAudioGate) ---

	// Звуки, поставленные на паузу ИМЕННО НАМИ. Возвращаем к жизни только их: остальное мог
	// приостановить кто-то другой, и «оживлять» чужое мы не вправе.
	TArray<TWeakObjectPtr<class UAudioComponent>> MutedWorldSounds;

	// Признак «мир сейчас заглушен нами» — чтобы не пересчитывать состояние каждый кадр.
	bool bWorldAudioMutedByMenu = false;

	// Когда в последний раз проверяли мир на новые звуки (живое время, идёт и на паузе).
	double LastWorldAudioSweepTime = -1000.0;

	// Виджет меню паузы: создаётся лениво при первом открытии, дальше переиспользуется.
	UPROPERTY()
	TObjectPtr<UPauseMenuWidget> PauseMenuWidget;

	// Виджет стартового экрана (Б3): создаётся лениво, максимум один раз за сессию (либо
	// «Продолжить», либо «Новая игра» — повторно экран не открывается).
	UPROPERTY()
	TObjectPtr<UStartScreenWidget> StartScreenWidget;

	// Виджет экрана настроек (ADR-062, подход 2): создаётся лениво при первом открытии,
	// дальше переиспользуется (значения перечитываются при каждом показе).
	UPROPERTY()
	TObjectPtr<USettingsScreenWidget> SettingsScreenWidget;

	// Ближайший староста (выставляется его overlap-триггером). null — старосты рядом нет.
	UPROPERTY()
	AElderNPC* NearbyElder = nullptr;

	// --- Рантайм-состояние интро (Build 1) ---

	// Интро уже пытались запустить (решение принимается один раз, когда появилась пешка).
	bool bIntroChecked = false;

	// Текущая фаза интро (None — не идёт).
	EIntroPhase IntroPhase = EIntroPhase::None;

	// Общий таймер интро от старта (с) — по нему считаются фазы строк/проявления.
	float IntroElapsed = 0.0f;

	// Таймер авто-подхода от начала проявления мира (с) — до передачи управления.
	float IntroAutoWalkElapsed = 0.0f;

	// Повторный заход → доступен hold-to-skip (на новой игре — нет).
	bool bIntroSkippable = false;

	// Интро пропущено удержанием → грейд-арку не ведём (сразу нормальный кадр).
	bool bIntroSkipped = false;

	// Накоплено удержания клавиши/пальца пропуска (с).
	float IntroSkipHeld = 0.0f;

	// Подавлять ввод движения игрока (во время авто-подхода). Проверяется в Move().
	bool bIntroInputLocked = false;

	// Мировая точка центра деревни (куда ведёт авто-подход и стрелка). Валидна при bIntroHasVillage.
	FVector IntroVillageLocation = FVector::ZeroVector;
	bool bIntroHasVillage = false;

	// Начальная дистанция до деревни (для грейд-арки: чем ближе, тем светлее/насыщеннее).
	float IntroInitialDistance = 0.0f;

	// Актор-цель деревни (стрелка направления интро; фолбэк — ближайший староста).
	UPROPERTY()
	TObjectPtr<AActor> IntroVillageActor = nullptr;

	// Экран интро (чёрный фон + строки), строится кодом (UIntroScreenWidget).
	UPROPERTY()
	TObjectPtr<UIntroScreenWidget> IntroWidget = nullptr;

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
