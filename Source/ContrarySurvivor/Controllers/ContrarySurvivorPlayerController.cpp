// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivorPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputCoreTypes.h" // EKeys (детект Shift для слайдера ±10)
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ARangedWeapon.h"
#include "UInventoryComponent.h"      // QA-харнесс: рюкзак (использовать/выбросить/продать)
#include "AMasterInventoryItem.h"     // QA-харнесс: предмет + EItemCategory
#include "AQuestItem.h"               // QA-харнесс (Фаза 5): выдача квест-предметов (C/X)
#include "Kismet/GameplayStatics.h"   // QA-харнесс: DeleteGameInSlot (очистка сейва)
#include "Engine/HitResult.h"
#include "Engine/DamageEvents.h" // QA: FDamageEvent (force-kill N через TakeDamage)
#include "EngineUtils.h" // TActorIterator
#include "GameFramework/Character.h"                  // QA: ACharacter (телепорт V)
#include "GameFramework/CharacterMovementComponent.h" // QA: StopMovementImmediately (телепорт V)
#include "Components/CapsuleComponent.h"              // QA: halfHeight капсулы (телепорт V)
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h"
#include "ContrarySurvivor/Actors/ShopTypes.h"          // FShopEntry (каталог в OnQABuyCheapest, A2)
#include "ContrarySurvivor/Actors/ShopVendor.h"         // IShopVendor / UShopVendor (вендор магазина, A2)
#include "ContrarySurvivor/Actors/ElderNPC.h"           // Фаза 5: староста (диалог/квест)
#include "ContrarySurvivor/Components/QuestComponent.h"  // Фаза 5: журнал квестов игрока
#include "ContrarySurvivor/Components/CorpseLootComponent.h" // Build 1.2.1 (А1): обыск трупов
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"   // QA: спавн тест-волка (клавиша B)
#include "ContrarySurvivor/Subsystems/SpawnPlacementUtils.h" // QA: трасса до пола (телепорт V)
#include "ContrarySurvivor/Controllers/EnemyAIController.h" // QA headless-тест погони: режим/дальность
#include "NavigationSystem.h"  // QA headless-тест: проекция враг/игрок на навмеш (selfNav/targetNav)
#include "TimerManager.h"      // QA headless-тест: таймер-сэмплинг
#include "ContrarySurvivor/Debug/QADebug.h"              // QA debug-флаги/хелпер (J/U/B/O/N/V)
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Engine/Engine.h"                      // GEngine->Exec (подавление экранного спама)
#include "ContrarySurvivor/Retention/OnboardingComponent.h" // Этап F1: онбординг-подсказки
#include "ContrarySurvivor/Retention/DailyRewardComponent.h" // Build 1: отложенное окно ежедневки (после интро-диалога)
#include "ContrarySurvivor/UI/TouchControlsWidget.h"        // Этап G: виртуальный стик (Android)
#include "ContrarySurvivor/UI/PauseMenuWidget.h"            // Этап G: меню паузы
#include "ContrarySurvivor/UI/StartScreenWidget.h"          // Главное меню (ADR-062; вырос из стартового экрана Б3)
#include "ContrarySurvivor/UI/SettingsScreenWidget.h"       // Экран настроек (ADR-062, подход 2)
#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h" // применение настроек при запуске
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"  // маркер «игра уже запускалась» (меню со второго запуска)
#include "ContrarySurvivor/UI/IntroScreenWidget.h"          // Build 1: экран интро (чёрный + строки)
#include "Blueprint/UserWidget.h"                            // CreateWidget
#include "Kismet/KismetSystemLibrary.h"                      // QuitGame («Выход» меню паузы)

// Пространство имён переводов для литералов этого файла (ADR-050): подписи тач-кнопок.
#define LOCTEXT_NAMESPACE "ContrarySurvivorPlayerController"

AContrarySurvivorPlayerController::AContrarySurvivorPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentTarget = nullptr;

	// Дефолтная раскладка тач-кнопок (этап G, шаг 2): веер правого-нижнего угла под большой
	// палец — ОГОНЬ в углу крупный, ДЕЙСТВИЕ левее, ПЕРЕЗАРЯД выше, БЕГ по диагонали,
	// ОРУЖИЕ над перезарядкой; СУМКА — правый-верх, ПАУЗА — малозаметная в левом-верхнем.
	// Margin = отступ ЦЕНТРА кнопки от своего угла (px). Ринат тюнит в редакторе (EditAnywhere).
	TouchFireButton.Margin      = FVector2D(170.0f, 170.0f); TouchFireButton.Radius      = 75.0f;
	TouchInteractButton.Margin  = FVector2D(370.0f, 150.0f); TouchInteractButton.Radius  = 55.0f;
	TouchReloadButton.Margin    = FVector2D(150.0f, 370.0f); TouchReloadButton.Radius    = 50.0f;
	TouchSprintButton.Margin    = FVector2D(340.0f, 320.0f); TouchSprintButton.Radius    = 50.0f;
	TouchWeaponButton.Margin    = FVector2D(150.0f, 540.0f); TouchWeaponButton.Radius    = 45.0f;
	TouchInventoryButton.Margin = FVector2D(120.0f, 100.0f); TouchInventoryButton.Radius = 50.0f;
	TouchPauseButton.Margin     = FVector2D(70.0f, 70.0f);   TouchPauseButton.Radius     = 32.0f;

	// Подписи кнопок (дефолты; были зашиты в BuildButtons виджета — теперь EditAnywhere-поле
	// FTouchButtonSettings.Label, Ринат меняет в BP без пересборки). Переводимые (ADR-050).
	TouchFireButton.Label      = LOCTEXT("TouchFire", "ОГОНЬ");
	TouchInteractButton.Label  = LOCTEXT("TouchInteract", "ДЕЙСТВИЕ");
	TouchReloadButton.Label    = LOCTEXT("TouchReload", "ПЕРЕЗАРЯД");
	TouchSprintButton.Label    = LOCTEXT("TouchSprint", "БЕГ");
	TouchWeaponButton.Label    = LOCTEXT("TouchWeapon", "ОРУЖИЕ");
	TouchInventoryButton.Label = LOCTEXT("TouchInventory", "СУМКА");
	TouchPauseButton.Label     = LOCTEXT("TouchPause", "II");

	// G2: enum зоны жеста в заголовке только forward-объявлен — значение доступно здесь.
	ShopTouchZone = EShopDragZone::None;
}

void AContrarySurvivorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

	// Настройки игрока (ADR-062, подход 2) применяем ДО создания тач-слоя: пресет качества,
	// масштаб разрешения и предел кадров должны действовать с первого кадра, а слой экранных
	// кнопок ниже сразу возьмёт из настроек прозрачность и чувствительность.
	// Значения движок уже прочитал с диска при создании объекта настроек — это и есть
	// «сохраняется между запусками» (Config/DefaultEngine.ini, GameUserSettingsClassName).
	if (UContrarySurvivorGameUserSettings* PlayerSettings = UContrarySurvivorGameUserSettings::Get())
	{
		PlayerSettings->ApplyContrarySettings();
	}
	else
	{
		UE_LOG(LogQA, Warning,
			TEXT("QA: player settings unavailable (GameUserSettingsClassName в DefaultEngine.ini?) — играем на значениях по умолчанию"));
	}

	// Показываем курсор мыши (нужен для выбора цели кликом)
	bShowMouseCursor = true;
	bEnableClickEvents = true;
	// Android (этап G): тапы должны доходить до актор/компонент-событий наравне с кликами.
	bEnableTouchEvents = true;

	// QA-харнесс (Фаза 4 раунд 2): гасим экранные сообщения движка («LIGHTING NEEDS TO BE
	// REBUILT» и т.п.), чтобы не мешали приёмке автотестером. Это косметика рендера; сам
	// Build Lighting — позже. DisableAllScreenMessages выставляет GAreScreenMessagesEnabled=false
	// (подтверждено в UE 5.5: UEngine::HandleDisableAllScreenMessagesCommand).
	if (GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("DisableAllScreenMessages"));
	}

	// Этап G (ADR-017): экранный тач-слой поверх той же абстракции ввода. На Android включён
	// всегда; на ПК — флагом bEnableTouchControls (тест мышью: клик по стику = имитация пальца).
	// Настройки копируются с контроллера. Слот TouchControlsWidgetClass назначен (ADR-048) —
	// дерево кнопок/стика приходит из WBP Рината; пуст — прежний кодовый виджет.
#if PLATFORM_ANDROID
	const bool bWantTouchLayer = true;
#else
	const bool bWantTouchLayer = bEnableTouchControls;
#endif
	if (bWantTouchLayer)
	{
		UClass* TouchLayerClass = TouchControlsWidgetClass
			? TouchControlsWidgetClass.Get()
			: UTouchControlsWidget::StaticClass();
		if (!MoveAction)
		{
			UE_LOG(LogQA, Warning, TEXT("QA: touch layer skipped — MoveAction is null on controller"));
		}
		else if (UTouchControlsWidget* Layer =
			CreateWidget<UTouchControlsWidget>(this, TouchLayerClass))
		{
			FTouchControlsConfig TouchConfig;
			TouchConfig.StickRadius      = TouchStickRadius;
			TouchConfig.StickThumbRadius = TouchStickThumbRadius;
			TouchConfig.StickMargin      = TouchStickMargin;
			TouchConfig.StickDeadZone    = TouchStickDeadZone;
			TouchConfig.IdleOpacity      = TouchIdleOpacity;
			TouchConfig.ActiveOpacity    = TouchActiveOpacity;
			TouchConfig.StickBaseColor   = TouchStickBaseColor;
			TouchConfig.StickThumbColor  = TouchStickThumbColor;
			TouchConfig.FireButton       = TouchFireButton;
			TouchConfig.ReloadButton     = TouchReloadButton;
			TouchConfig.InteractButton   = TouchInteractButton;
			TouchConfig.SprintButton     = TouchSprintButton;
			TouchConfig.WeaponButton     = TouchWeaponButton;
			TouchConfig.InventoryButton  = TouchInventoryButton;
			TouchConfig.PauseButton      = TouchPauseButton;
			TouchConfig.bSprintToggle    = bTouchSprintToggle;
			Layer->InitTouch(this, MoveAction, FireAction, ReloadAction, SprintAction, TouchConfig);
			// Поверх настроек контроллера — выбор игрока (прозрачность кнопок, чувствительность).
			Layer->ApplyPlayerSettings();
			// Z=10: под онбординг-подсказками (40) и модальными окнами (50/60).
			Layer->AddToViewport(/*ZOrder=*/10);
			TouchControlsLayer = Layer;
			UE_LOG(LogQA, Display, TEXT("QA: touch layer created (%s)"),
				TouchControlsWidgetClass
					? *FString::Printf(TEXT("WBP %s"), *TouchLayerClass->GetName())
					: TEXT("code-built tree"));
		}
	}
}

void AContrarySurvivorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		// Движение
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Move);

		// Спринт
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Sprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AContrarySurvivorPlayerController::Sprint);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SprintAction is null. Check Blueprint/Header assignment."));
		}

		// Действия
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Interact);
		EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Inventory);

		// Стрельба
		if (FireAction)
		{
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Fire);
			// BUG1: отпускание клика сбрасывает edge-флаг UI (один клик = одно UI-действие).
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Completed, this, &AContrarySurvivorPlayerController::OnFireReleased);
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Canceled, this, &AContrarySurvivorPlayerController::OnFireReleased);
		}

		// Перезарядка
		if (ReloadAction)
		{
			EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Reload);
		}
	}

	// LEGACY-привязка переключения оружия (пистолет<->нож). UEnhancedInputComponent
	// наследует UInputComponent, поэтому legacy ActionMapping из DefaultInput.ini работает
	// параллельно Enhanced Input — без создания нового IA/IMC .uasset (Фаза 3, no-editor путь).
	if (InputComponent)
	{
		InputComponent->BindAction(TEXT("SwitchWeapon"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnSwitchWeapon);

		// Инвентарь (Tab / I) — legacy ActionMapping "ToggleInventory" (Фаза 4, без нового .uasset).
		InputComponent->BindAction(TEXT("ToggleInventory"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnToggleInventory);

		// Взаимодействие (E) — legacy ActionMapping "Interact" (Фаза 4, экономика: магазин).
		InputComponent->BindAction(TEXT("Interact"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnInteract);

		// ==================================================================
		// ОТЛАДОЧНЫЕ КЛАВИШИ — ТОЛЬКО ВНЕ ПУБЛИКАЦИОННОЙ СБОРКИ (Б5 задания издателя).
		// В режиме Shipping выключатель CONTRARY_WITH_QA_CHEATS равен нулю, и весь блок
		// привязок не компилируется: клавиши не «молчат», их просто нет в сборке. Сами
		// строки привязок остаются в Config/DefaultInput.ini — без обработчика они не
		// делают ничего, и удалять их незачем (в разработке они нужны).
		// ==================================================================
#if CONTRARY_WITH_QA_CHEATS
		// QA-харнесс (Фаза 4 раунд 2): тест-действия F1-F4 + M (деньги; перевешено с F5 из-за
		// вьюмода Shader Complexity) + T (телепорт к торговцу). Legacy ActionMapping,
		// Config/DefaultInput.ini. Дают автотестеру (Computer Use) проверять без `~`-консоли.
		InputComponent->BindAction(TEXT("QAToggleDebugCam"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnToggleDebugCamera);
		InputComponent->BindAction(TEXT("QAGiveItems"),      IE_Pressed, this, &AContrarySurvivorPlayerController::OnTestGiveItems);
		InputComponent->BindAction(TEXT("QAEquipArmor"),     IE_Pressed, this, &AContrarySurvivorPlayerController::OnTestEquipArmor);
		InputComponent->BindAction(TEXT("QAUnequipArmor"),   IE_Pressed, this, &AContrarySurvivorPlayerController::OnTestUnequipArmor);
		InputComponent->BindAction(TEXT("QAGiveMoney"),      IE_Pressed, this, &AContrarySurvivorPlayerController::OnTestGiveMoney);
			// Тест-телепорт к торговцу (клавиша T) — обход блокировки волками для проверки купли/продажи.
			InputComponent->BindAction(TEXT("QATeleportToTrader"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnQATeleportToTrader);

		// QA-харнесс (Фаза 4 раунд 3): дублёры UI-действий клавишами (тестер не кликает HUD в PIE).
		InputComponent->BindAction(TEXT("QAUseConsumable"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAUseFirstConsumable);
		InputComponent->BindAction(TEXT("QADropItem"),      IE_Pressed, this, &AContrarySurvivorPlayerController::OnQADropFirstItem);
		InputComponent->BindAction(TEXT("QABuyCheapest"),   IE_Pressed, this, &AContrarySurvivorPlayerController::OnQABuyCheapest);
		InputComponent->BindAction(TEXT("QASellFirst"),     IE_Pressed, this, &AContrarySurvivorPlayerController::OnQASellFirstItem);
		InputComponent->BindAction(TEXT("QAClearSave"),     IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAClearSave);

		// QA-харнесс (Фаза 5): квесты/диалог на буквенных клавишах (Y/G/H/K), legacy ActionMapping.
		InputComponent->BindAction(TEXT("QATeleportToElder"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnQATeleportToElder);
		InputComponent->BindAction(TEXT("QAAcceptQuest"),     IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAAcceptQuest);
		InputComponent->BindAction(TEXT("QATurnInQuest"),     IE_Pressed, this, &AContrarySurvivorPlayerController::OnQATurnInQuest);
		InputComponent->BindAction(TEXT("QACreditWolfKill"),  IE_Pressed, this, &AContrarySurvivorPlayerController::OnQACreditWolfKill);

			// QA-харнесс (Фаза 5, демка-квесты): C — выдать игроку 5 «Шкур волка» (тест сдачи кв.1);
			// X — выдать «Ноутбук» (тест сдачи кв.2). Сборщик не может фармить лут вручную.
			InputComponent->BindAction(TEXT("QAGiveWolfHides"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAGiveWolfHides);
			InputComponent->BindAction(TEXT("QAGiveNotebook"),  IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAGiveNotebook);

		// QA debug-инструменты (Фаза 5): god/forcedrop/spawn-wolf/overlay (J/U/B/O), legacy ActionMapping.
		InputComponent->BindAction(TEXT("QAGodMode"),      IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAToggleGodMode);
		InputComponent->BindAction(TEXT("QAForceDrop"),    IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAToggleForceDrop);
		InputComponent->BindAction(TEXT("QASpawnWolf"),    IE_Pressed, this, &AContrarySurvivorPlayerController::OnQASpawnTestWolf);
		InputComponent->BindAction(TEXT("QAToggleOverlay"),IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAToggleOverlay);

		// QA debug-инструмент (Фаза 5, доп.): N — force-kill ближайшего врага.
		InputComponent->BindAction(TEXT("QAForceKill"),     IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAForceKillNearest);

		// P (QA, #26): мгновенно убить игрока для теста экрана смерти.
		InputComponent->BindAction(TEXT("QAKillPlayer"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAKillPlayer);
#endif // CONTRARY_WITH_QA_CHEATS

		// #26: возрождение по клавише (Enter / Пробел) на экране смерти — дубль кнопки «Возродиться».
		InputComponent->BindAction(TEXT("Respawn"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnRespawnPressed);

		// Этап G: меню паузы (Esc / L / Android Back, legacy ActionMapping "PauseMenu").
		// bExecuteWhenPaused — иначе при поставленной паузе клавиша закрытия не сработала бы
		// (ввод при паузе обрабатывается, но только привязки с этим флагом).
		FInputActionBinding& PauseBinding = InputComponent->BindAction(TEXT("PauseMenu"), IE_Pressed,
			this, &AContrarySurvivorPlayerController::OnTogglePauseMenu);
		PauseBinding.bExecuteWhenPaused = true;

		// Этап G: тап по экрану Android (Touch1..Touch3, legacy ActionMapping "ScreenTap") —
		// заводится в тот же Fire()-путь, что клик ЛКМ (обоснование — коммент у OnScreenTapPressed).
		InputComponent->BindAction(TEXT("ScreenTap"), IE_Pressed,  this, &AContrarySurvivorPlayerController::OnScreenTapPressed);
		InputComponent->BindAction(TEXT("ScreenTap"), IE_Released, this, &AContrarySurvivorPlayerController::OnScreenTapReleased);

		// G2: тач-жесты магазина (свайп-прокрутка списков, слайдер пальцем, тап = клик на
		// отпускании). BindTouch даёт индекс пальца и ЖИВУЮ позицию каждого события — в отличие
		// от ScreenTap (клавиша без позиции). Исполняется тем же конвейером PlayerInput, что и
		// легаси-экшены (UPlayerInput::ProcessInputStack, TouchBindings — PlayerInput.cpp:1382),
		// под EnhancedPlayerInput работает. На ПК тач-события не генерятся — мышь не задета.
		InputComponent->BindTouch(IE_Pressed,  this, &AContrarySurvivorPlayerController::OnShopTouchPressed);
		InputComponent->BindTouch(IE_Repeat,   this, &AContrarySurvivorPlayerController::OnShopTouchMoved);
		InputComponent->BindTouch(IE_Released, this, &AContrarySurvivorPlayerController::OnShopTouchReleased);

		// Фаза 5: слайдер количества в магазине — ±количество (стрелки/колесо, Shift=±10).
		InputComponent->BindAction(TEXT("ShopQtyDec"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnShopQtyDec);
		InputComponent->BindAction(TEXT("ShopQtyInc"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnShopQtyInc);

		// Этап F (онбординг): любой ввод скрывает активную подсказку. bConsumeInput=false —
		// нажатие НЕ съедается, геймплей/QA-клавиши работают как раньше.
		FInputKeyBinding& AnyKeyBinding = InputComponent->BindKey(EKeys::AnyKey, IE_Pressed,
			this, &AContrarySurvivorPlayerController::OnAnyInputForHints);
		AnyKeyBinding.bConsumeInput = false;
	}
}

void AContrarySurvivorPlayerController::OnAnyInputForHints()
{
	if (UOnboardingComponent* OnboardingComp = GetOnboarding())
	{
		OnboardingComp->DismissCurrentHint();
	}
}

UOnboardingComponent* AContrarySurvivorPlayerController::GetOnboarding() const
{
	const APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	return PlayerChar ? PlayerChar->GetOnboarding() : nullptr;
}

// ---------------------------------------------------------------------------
// Экран смерти (#26): показ/скрытие + возрождение по клавише + QA-убийство игрока
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::ShowDeathScreen()
{
	bDeathScreen = true;
	bUIClickConsumed = false;

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetDeathScreenOpen(true);
	}

	// Режим ввода UI (клик уходит в кнопку «Возродиться»); геймплей-экшены подавлены флагом bDeathScreen.
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	FQADebug::QA(this, TEXT("QA: death screen opened (input disabled)"), /*bScreen=*/true);

	// Этап F (онбординг): первая смерть — подсказка со СТРОГОЙ формулировкой ADR-044 п.3
	// (без «можно вернуться и забрать»). Сам экран смерти не трогаем.
	if (UOnboardingComponent* OnboardingComp = GetOnboarding())
	{
		OnboardingComp->TryShowHint(EOnboardingHint::Death);
	}
}

void AContrarySurvivorPlayerController::HideDeathScreen()
{
	if (!bDeathScreen)
	{
		return;
	}
	bDeathScreen = false;
	bUIClickConsumed = false;

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetDeathScreenOpen(false);
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = true; // курсор нужен в игре (клик-таргетинг)
}

// ---------------------------------------------------------------------------
// Меню паузы (этап G): пауза + «Продолжить» + «Выход» (меню-минимум, решение game-lead)
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OnTogglePauseMenu()
{
	// Пока не решено «Продолжить»/«Новая игра» — меню паузы поверх стартового экрана не нужно.
	if (bStartScreenOpen)
	{
		return;
	}
	// На экране смерти меню не открываем — там свой модальный флоу («Возродиться»).
	if (bDeathScreen)
	{
		return;
	}
	// Build 1.2.1 (ТЗ А1): Esc при открытом окне обыска трупа ЗАКРЫВАЕТ его (требование
	// «закрытие Esc/крестик»), а не открывает меню паузы поверх.
	if (bCorpseLootOpen)
	{
		CloseCorpseLoot();
		return;
	}
	if (bPauseMenuOpen)
	{
		ClosePauseMenu();
	}
	else
	{
		OpenPauseMenu();
	}
}

void AContrarySurvivorPlayerController::OpenPauseMenu()
{
	if (bPauseMenuOpen)
	{
		return;
	}

	if (!PauseMenuWidget)
	{
		// Слот назначен (ADR-048) — окно из WBP, правится в дизайнере; пусто — кодовое дерево.
		PauseMenuWidget = CreateWidget<UPauseMenuWidget>(this,
			PauseMenuWidgetClass ? PauseMenuWidgetClass.Get() : UPauseMenuWidget::StaticClass());
		if (!PauseMenuWidget)
		{
			return;
		}
		PauseMenuWidget->ApplyStyle(PauseMenuStyle); // стиль с контроллера (EditAnywhere) поверх дефолтов
		PauseMenuWidget->OnResumeRequested.AddUObject(this, &AContrarySurvivorPlayerController::ClosePauseMenu);
		PauseMenuWidget->OnQuitRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandlePauseQuit);
	}
	// Z=60: поверх окна ежедневки (50) и остального UI.
	PauseMenuWidget->AddToViewport(/*ZOrder=*/60);

	bPauseMenuOpen = true;
	bUIClickConsumed = false;

	// Тач-слой прячем: стик не рисуется поверх затемнения, зажатый стик сбрасывается.
	if (TouchControlsLayer)
	{
		TouchControlsLayer->SetLayerEnabled(false);
	}

	// Пауза мира. Кнопки меню живут: Slate игровой паузой не останавливается, а геймплейные
	// Enhanced Input-экшены при паузе молчат (bTriggerWhenPaused=false по умолчанию у UInputAction).
	SetPause(true);

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	UE_LOG(LogQA, Display, TEXT("QA: pause menu OPEN (world paused)"));
}

void AContrarySurvivorPlayerController::ClosePauseMenu()
{
	if (!bPauseMenuOpen)
	{
		return;
	}
	bPauseMenuOpen = false;
	bUIClickConsumed = false;

	if (PauseMenuWidget)
	{
		PauseMenuWidget->RemoveFromParent();
	}

	SetPause(false);

	if (TouchControlsLayer)
	{
		TouchControlsLayer->SetLayerEnabled(true);
	}

	// Пауза могла открыться поверх другой модалки (инвентарь/магазин/диалог) — режим ввода
	// возвращаем в Game только если модальных окон не осталось.
	if (!IsAnyModalUIOpen())
	{
		SetInputMode(FInputModeGameOnly());
	}
	bShowMouseCursor = true;

	UE_LOG(LogQA, Display, TEXT("QA: pause menu CLOSED (world resumed)"));
}

void AContrarySurvivorPlayerController::HandlePauseQuit()
{
	UE_LOG(LogQA, Display, TEXT("QA: pause menu QUIT pressed — quitting game"));
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

// ---------------------------------------------------------------------------
// Главное меню (ADR-062, спека glavnoe-menu-spec.md; выросло из стартового экрана Б3):
// показывается со ВТОРОГО запуска игры (решает MaybeStartIntro). Паттерн — точная копия
// меню паузы (SetPause + барьер виджета), см. OpenPauseMenu/ClosePauseMenu выше.
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OpenStartScreen()
{
	if (bStartScreenOpen)
	{
		return;
	}

	if (!StartScreenWidget)
	{
		// Слот назначен (ADR-048) — окно из WBP, правится в дизайнере; пусто — кодовое дерево.
		StartScreenWidget = CreateWidget<UStartScreenWidget>(this,
			StartScreenWidgetClass ? StartScreenWidgetClass.Get() : UStartScreenWidget::StaticClass());
		if (!StartScreenWidget)
		{
			// Виджет не создался — не блокируем игру навсегда, откатываемся к обычной новой игре.
			UE_LOG(LogQA, Warning, TEXT("QA: start screen widget creation failed — falling back to new game"));
			StartNewGameFlow();
			return;
		}
		StartScreenWidget->ApplyStyle(StartScreenStyle); // стиль с контроллера (EditAnywhere) поверх дефолтов
		StartScreenWidget->OnContinueRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleStartScreenContinue);
		StartScreenWidget->OnNewGameRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleStartScreenNewGame);
		StartScreenWidget->OnExitRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleStartScreenExit);
		// Подход 2 волны меню: обработчик появился — и вместе с ним появился сам пункт
		// «Настройки». Виджет держит пункт спрятанным, пока OnSettingsRequested никем не
		// привязан (UStartScreenWidget::ApplyMenuRowVisibility), правок виджета не потребовалось.
		StartScreenWidget->OnSettingsRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleStartScreenSettings);
	}

	// Наличие сейва освежаем при КАЖДОМ открытии (меню без сейва: «Продолжить» не
	// показывается вовсе, «Новая игра» стартует без переспроса — спека).
	APlayerCharacter* MenuPawn = Cast<APlayerCharacter>(GetPawn());
	StartScreenWidget->SetHasSave(MenuPawn && MenuPawn->HasSaveGame());

	// Z=70: выше меню паузы (60) и интро (50) — при интро экран не появляется, запас на будущее.
	StartScreenWidget->AddToViewport(/*ZOrder=*/70);

	bStartScreenOpen = true;
	bUIClickConsumed = false;

	if (TouchControlsLayer)
	{
		TouchControlsLayer->SetLayerEnabled(false);
	}

	// Пауза мира — как меню паузы: геймплейные Enhanced Input-экшены молчат (bTriggerWhenPaused
	// по умолчанию false), кнопки виджета работают (Slate игровой паузой не останавливается).
	SetPause(true);

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	UE_LOG(LogQA, Display, TEXT("QA: start screen OPEN (has save: %s, world paused)"),
		(MenuPawn && MenuPawn->HasSaveGame()) ? TEXT("yes") : TEXT("no"));
}

void AContrarySurvivorPlayerController::CloseStartScreen()
{
	if (!bStartScreenOpen)
	{
		return;
	}
	bStartScreenOpen = false;
	bUIClickConsumed = false;

	if (StartScreenWidget)
	{
		StartScreenWidget->RemoveFromParent();
	}

	SetPause(false);

	if (TouchControlsLayer)
	{
		TouchControlsLayer->SetLayerEnabled(true);
	}

	if (!IsAnyModalUIOpen())
	{
		SetInputMode(FInputModeGameOnly());
	}
	bShowMouseCursor = true;

	UE_LOG(LogQA, Display, TEXT("QA: start screen CLOSED (world resumed)"));
}

void AContrarySurvivorPlayerController::HandleStartScreenContinue()
{
	CloseStartScreen();

	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	const bool bLoaded = PlayerChar && PlayerChar->LoadGameForContinue();
	UE_LOG(LogQA, Display, TEXT("QA: start screen -> CONTINUE (%s)"),
		bLoaded ? TEXT("save loaded") : TEXT("load FAILED — stayed at spawn defaults"));

	// Б3, замечание 9 ревизии: интро НЕ играет при «Продолжить» — IntroPhase остаётся None.
	// Но если загрузились ПОСРЕДИ интро-этапа (журнал квестов пуст — до старосты не дошли),
	// игрок обязан снова видеть задачу и стрелку, иначе после загрузки некуда идти.
	if (bLoaded)
	{
		ResumeIntroObjectiveAfterContinue();
	}
}

void AContrarySurvivorPlayerController::HandleStartScreenNewGame()
{
	CloseStartScreen();

	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->ResetToNewGame();
	}

	StartNewGameFlow(); // полное интро — как у игрока без сейва вообще
}

void AContrarySurvivorPlayerController::HandleStartScreenExit()
{
	// «Выход» главного меню закрывает игру — тот же путь, что «Выход» меню паузы.
	UE_LOG(LogQA, Display, TEXT("QA: start screen EXIT pressed — quitting game"));
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

// ---------------------------------------------------------------------------
// Экран настроек (ADR-062, подход 2 волны меню; спека glavnoe-menu-spec.md).
// Открывается пунктом «Настройки» ПОВЕРХ главного меню: мир к этому моменту уже на паузе,
// поэтому пауза здесь не «своя», а унаследованная — снимаем её только если ставили сами.
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::HandleStartScreenSettings()
{
	OpenSettingsScreen();
}

void AContrarySurvivorPlayerController::OpenSettingsScreen()
{
	if (bSettingsScreenOpen)
	{
		return;
	}

	if (!SettingsScreenWidget)
	{
		// Слот назначен (ADR-048) — окно из WBP_Settings; пусто — кодовое дерево.
		SettingsScreenWidget = CreateWidget<USettingsScreenWidget>(this,
			SettingsScreenWidgetClass ? SettingsScreenWidgetClass.Get() : USettingsScreenWidget::StaticClass());
		if (!SettingsScreenWidget)
		{
			// Экран не создался — молча остаёмся в меню, игру не блокируем.
			UE_LOG(LogQA, Warning, TEXT("QA: settings screen widget creation failed — staying in menu"));
			return;
		}
		SettingsScreenWidget->OnCloseRequested.AddUObject(this, &AContrarySurvivorPlayerController::CloseSettingsScreen);
		SettingsScreenWidget->OnSettingsChanged.AddUObject(this, &AContrarySurvivorPlayerController::ApplyPlayerSettingsToWorld);
		SettingsScreenWidget->OnResetProgressConfirmed.AddUObject(this, &AContrarySurvivorPlayerController::HandleSettingsResetProgress);
	}

	// Значения перечитываются при каждом показе (виджет переиспользуется).
	SettingsScreenWidget->RefreshFromSettings();

	// Z=75: выше главного меню (70) — настройки открываются поверх него, меню остаётся под ними.
	SettingsScreenWidget->AddToViewport(/*ZOrder=*/75);

	bSettingsScreenOpen = true;
	bUIClickConsumed = false;

	if (TouchControlsLayer)
	{
		TouchControlsLayer->SetLayerEnabled(false);
	}

	// Пауза уже стоит (экран открыт из меню) — второй раз не ставим и, главное, при закрытии
	// не снимаем чужую: иначе «Назад» из настроек оживил бы мир под открытым меню.
	bPausedBySettingsScreen = !IsPaused();
	if (bPausedBySettingsScreen)
	{
		SetPause(true);
	}

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	UE_LOG(LogQA, Display, TEXT("QA: settings screen OPEN"));
}

void AContrarySurvivorPlayerController::CloseSettingsScreen()
{
	if (!bSettingsScreenOpen)
	{
		return;
	}
	bSettingsScreenOpen = false;
	bUIClickConsumed = false;

	if (SettingsScreenWidget)
	{
		SettingsScreenWidget->RemoveFromParent();
	}

	if (bPausedBySettingsScreen)
	{
		SetPause(false);
		bPausedBySettingsScreen = false;
	}

	// Тач-слой возвращаем только если под настройками не осталось другого модального окна
	// (штатный путь: под ними главное меню — слой обязан остаться выключенным).
	if (TouchControlsLayer && !IsAnyModalUIOpen())
	{
		TouchControlsLayer->SetLayerEnabled(true);
	}

	if (!IsAnyModalUIOpen())
	{
		SetInputMode(FInputModeGameOnly());
	}
	bShowMouseCursor = true;

	UE_LOG(LogQA, Display, TEXT("QA: settings screen CLOSED"));
}

void AContrarySurvivorPlayerController::ApplyPlayerSettingsToWorld()
{
	// Графику, предел кадров и запись на диск делает сам объект настроек. Здесь — только то,
	// что живёт в мире и объекту настроек недоступно.
	if (TouchControlsLayer)
	{
		TouchControlsLayer->ApplyPlayerSettings();
	}
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->ApplyAudioSettings();
	}
}

void AContrarySurvivorPlayerController::HandleSettingsResetProgress()
{
	// Сюда попадаем ТОЛЬКО после двойного переспроса (виджет держит стадии сам).
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->ResetToNewGame();
	}
	UE_LOG(LogQA, Display, TEXT("QA: settings RESET PROGRESS — save wiped"));

	// Экран настроек закрываем: игрок вернулся в меню, и меню обязано показать правду —
	// сохранения больше нет, пункта «Продолжить» быть не должно.
	CloseSettingsScreen();
	if (StartScreenWidget)
	{
		StartScreenWidget->SetHasSave(false);
	}
}

bool AContrarySurvivorPlayerController::ShouldShowMainMenuOnLaunch(bool bLaunchedBefore, bool bHasSave)
{
	// Спека главного меню: самый первый запуск после установки — сразу во вступление, со
	// второго запуска — меню. Найденный сейв тоже доказывает прошлый запуск (обновление со
	// сборки, где маркера запусков ещё не было) — прежнее поведение Б3 сохраняется.
	return bLaunchedBefore || bHasSave;
}

// ---------------------------------------------------------------------------
// Тап по экрану Android (этап G): ScreenTap = Touch1..Touch3 -> тот же путь, что клик ЛКМ
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OnScreenTapPressed()
{
	// G2: тапы в ОТКРЫТОМ МАГАЗИНЕ обрабатывает конвейер жестов (OnShopTouch*) на ОТПУСКАНИИ
	// пальца — клик на нажатии превращал бы каждый свайп в покупку того, что под пальцем.
	// Путь мыши не задет: ScreenTap стреляет только от Touch-клавиш, которых на ПК нет.
	if (bShopOpen)
	{
		return;
	}

	// Один тап = один «клик»: выбор цели + выстрел по миру ЛИБО клик по открытому окну
	// Canvas-HUD (гейты и edge-логика — внутри Fire, позиция — из кеша курсора = точка тапа).
	// Очередь на удержание даёт отдельная экранная кнопка ОГОНЬ (инжекция IA_Fire).
	Fire(FInputActionValue(true));
}

void AContrarySurvivorPlayerController::OnScreenTapReleased()
{
	// Палец поднят = отпускание «клика»: сброс edge-флага UI (как Completed у IA_Fire).
	OnFireReleased(FInputActionValue(false));
}

// ---------------------------------------------------------------------------
// Тач-жесты магазина (G2): свайп = прокрутка списков / ручка слайдера, тап = клик
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OnShopTouchPressed(ETouchIndex::Type FingerIndex, FVector Location)
{
	// Отслеживаем ОДИН палец — первый коснувшийся при открытом магазине. Второй палец
	// (например, большой на кнопке СУМКА в UMG) сюда не доходит — его съедает Slate; а
	// пришедший вторым по вьюпорту — игнорируется до отпускания первого.
	if (!bShopOpen || ShopTouchFinger != INDEX_NONE)
	{
		return;
	}
	ShopTouchFinger = static_cast<int32>(FingerIndex);
	ShopTouchStart = FVector2D(Location.X, Location.Y);
	ShopTouchLast = ShopTouchStart;
	bShopTouchDragging = false;
	ShopTouchZone = EShopDragZone::None;
}

void AContrarySurvivorPlayerController::OnShopTouchMoved(ETouchIndex::Type FingerIndex, FVector Location)
{
	if (static_cast<int32>(FingerIndex) != ShopTouchFinger)
	{
		return;
	}
	if (!bShopOpen)
	{
		ResetShopTouchState(); // магазин закрылся под пальцем (клавиша E) — жест мёртв
		return;
	}
	AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>();
	if (!CSHUD)
	{
		return;
	}

	const FVector2D Pos(Location.X, Location.Y);

	// Порог свайпа: зона фиксируется по ТОЧКЕ НАЧАЛА жеста (не по текущей) и дальше не
	// меняется — палец может выехать за список, прокрутка продолжается.
	if (!bShopTouchDragging && FVector2D::Distance(Pos, ShopTouchStart) >= TouchDragSlopPx)
	{
		bShopTouchDragging = true;
		ShopTouchZone = CSHUD->GetShopDragZone(ShopTouchStart);
	}

	if (bShopTouchDragging)
	{
		switch (ShopTouchZone)
		{
			case EShopDragZone::BuyList:
			case EShopDragZone::SellList:
				// Палец вверх (Y уменьшается) -> положительная дельта -> список листается вниз.
				CSHUD->ScrollShopZonePixels(ShopTouchZone, (ShopTouchLast.Y - Pos.Y) * TouchScrollSensitivity);
				break;
			case EShopDragZone::SliderTrack:
				CSHUD->SetShopSliderQtyFromX(Pos.X); // ручка следует за пальцем непрерывно
				break;
			default:
				break; // жест начат вне зон (фон панели/затемнение) — ничего не листаем
		}
	}

	ShopTouchLast = Pos;
}

void AContrarySurvivorPlayerController::OnShopTouchReleased(ETouchIndex::Type FingerIndex, FVector Location)
{
	if (static_cast<int32>(FingerIndex) != ShopTouchFinger)
	{
		return;
	}
	const bool bWasTap = !bShopTouchDragging;
	ResetShopTouchState();

	// Короткое касание без свайпа = клик по магазину в точке ОТПУСКАНИЯ (позиция BindTouch
	// точнее кеша курсора). Свайп кликом не заканчивается никогда.
	if (bShopOpen && bWasTap)
	{
		if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
		{
			CSHUD->HandleShopClick(FVector2D(Location.X, Location.Y));
		}
	}
}

void AContrarySurvivorPlayerController::ResetShopTouchState()
{
	ShopTouchFinger = INDEX_NONE;
	ShopTouchStart = FVector2D::ZeroVector;
	ShopTouchLast = FVector2D::ZeroVector;
	bShopTouchDragging = false;
	ShopTouchZone = EShopDragZone::None;
}

void AContrarySurvivorPlayerController::OnRespawnPressed()
{
	// Фаза 5: если в магазине открыт слайдер количества — Enter/Пробел подтверждает транзакцию.
	if (bShopOpen)
	{
		if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
		{
			if (CSHUD->IsShopSliderActive())
			{
				if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
				{
					CSHUD->ConfirmShopSlider(PlayerChar);
				}
				return;
			}
		}
	}

	// Дубль кнопки «Возродиться» клавишей (Enter / Пробел). Действует только на экране смерти.
	if (!bDeathScreen)
	{
		return;
	}
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		UE_LOG(LogQA, Display, TEXT("QA: respawn key pressed"));
		PlayerChar->Respawn();
	}
}

void AContrarySurvivorPlayerController::OnShopQtyDec()
{
	if (!bShopOpen) return;
	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		if (CSHUD->IsShopSliderActive())
		{
			const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
			CSHUD->AdjustShopSliderQty(bShift ? -10 : -1);
		}
		else
		{
			// Слайдер неактивен: колесо вниз (Dec) листает каталог «FOR SALE» вниз
			// (каталог перерос панель — 18 позиций). Стрелка Left — тоже, приемлемый бонус.
			CSHUD->ScrollShopList(1);
		}
	}
}

void AContrarySurvivorPlayerController::OnShopQtyInc()
{
	if (!bShopOpen) return;
	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		if (CSHUD->IsShopSliderActive())
		{
			const bool bShift = IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
			CSHUD->AdjustShopSliderQty(bShift ? 10 : 1);
		}
		else
		{
			// Колесо вверх (Inc) листает каталог вверх.
			CSHUD->ScrollShopList(-1);
		}
	}
}

// ===========================================================================
// ОТЛАДОЧНЫЕ КЛАВИШИ: тела обработчиков. Весь блок до парного #endif ниже в
// публикационной сборке не компилируется (Б5 задания издателя) — объявления в заголовке
// закрыты тем же выключателем CONTRARY_WITH_QA_CHEATS.
// ===========================================================================
#if CONTRARY_WITH_QA_CHEATS

void AContrarySurvivorPlayerController::OnQAKillPlayer()
{
	// P: мгновенно убить игрока штатным летальным уроном -> сработает экран смерти.
	if (FQADebug::bGodMode)
	{
		FQADebug::QA(this, TEXT("QA: QAKillPlayer skipped - god mode on"), /*bScreen=*/true);
		return;
	}

	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerChar)
	{
		FQADebug::QA(this, TEXT("QA: QAKillPlayer skipped - no player pawn"), /*bScreen=*/true);
		return;
	}

	// Летальный урон штатным путём (как враг/оружие). DamageCauser = сам игрок, чтобы не
	// перетереть «от кого погиб» (в TakeDamage само-урон игнорируется для LastDamagerName).
	FDamageEvent DamageEvent;
	PlayerChar->TakeDamage(1000000.0f, DamageEvent, this, PlayerChar);

	FQADebug::QA(this, TEXT("QA: QAKillPlayer"), /*bScreen=*/true);
}

// ---------------------------------------------------------------------------
// QA debug-инструменты (Фаза 5): god-mode / force-drop / spawn-wolf / overlay
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OnQAToggleGodMode()
{
	// J: тумблер неуязвимости + заморозки деградации голода/жажды. Включаем оверлей вместе
	// с god-mode, чтобы тестер сразу видел статус на экране.
	//
	// ЗАДАЧА 4 (отчёт QA «GODMODE сам выключился сразу после включения»): дебаунс против
	// двойного IE_Pressed (повтор клавиши от Computer Use / дребезг). Повторный тоггл в
	// пределах GodModeToggleDebounce сек после предыдущего ИГНОРИРУЕМ — иначе «вкл→тут же выкл».
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (Now - LastGodModeToggleTime < GodModeToggleDebounce)
	{
		FQADebug::QA(this, FString::Printf(TEXT("QA: GODMODE toggle ignored (debounce %.2fs)"),
			Now - LastGodModeToggleTime), /*bScreen=*/true);
		return;
	}
	LastGodModeToggleTime = Now;

	FQADebug::bGodMode = !FQADebug::bGodMode;
	if (FQADebug::bGodMode)
	{
		FQADebug::bOverlayVisible = true;
	}
	FQADebug::QA(this, FString::Printf(TEXT("QA: GODMODE %s"), FQADebug::bGodMode ? TEXT("on") : TEXT("off")), /*bScreen=*/true);
}

void AContrarySurvivorPlayerController::OnQAToggleForceDrop()
{
	// U: тумблер 100%-дропа со всех врагов.
	FQADebug::bForceDrop = !FQADebug::bForceDrop;
	FQADebug::QA(this, FString::Printf(TEXT("QA: FORCEDROP %s"), FQADebug::bForceDrop ? TEXT("on") : TEXT("off")), /*bScreen=*/true);
}

void AContrarySurvivorPlayerController::OnQASpawnTestWolf()
{
	// B: заспавнить одного тест-волка чуть впереди игрока (быстро убить и проверить лут).
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		FQADebug::QA(this, TEXT("QA: spawn test wolf skipped - no pawn/world"), /*bScreen=*/true);
		return;
	}

	// Точка ВПЛОТНУЮ перед игроком (300 ед.), чтобы волк сразу агрился/локался и был
	// достижим (kill->drop->подбор в одной точке). Высоту берём НА ПОЛУ трассой (как
	// спавн-сабсистемы, ZOffset=90 = центр капсулы над полом), а не фикс. +90 от Z игрока —
	// иначе волк висел/проваливался и оказывался «далеко» (баг QA: dist ~21907).
	const FVector PawnLoc = ControlledPawn->GetActorLocation();
	const FVector AheadXY = PawnLoc + ControlledPawn->GetActorForwardVector() * 300.0f;
	const float SpawnZ = SpawnPlacement::ResolveSpawnZ(
		World, AheadXY.X, AheadXY.Y, /*ZOffset=*/90.0f, TEXT("QATestWolf"), ControlledPawn);
	const FVector SpawnLoc(AheadXY.X, AheadXY.Y, SpawnZ);
	const FRotator SpawnRot = (PawnLoc - SpawnLoc).Rotation();

	// БАГ QA (dist 2352 вместо ~300): XY вычислялась верно (PawnLoc + Forward*300, по логу
	// SpawnLoc=(-1043,285,90) на полу, ~300 от игрока), но СПАВН-релокация уносила волка далеко.
	// AdjustIfPossibleButAlwaysSpawn при пересечении капсулы со статикой (в той XY floortrace
	// видел 45 хитов) синхронно зовёт FindTeleportSpot, который сдвигает актора на свободное
	// место — здесь на 2352 ед. (поэтому замер dist сразу после SpawnActor уже «далеко»).
	// В отличие от спавна у Логова, B НЕ проецирует точку на навмеш, поэтому свободного места
	// рядом нет. Для QA-инструмента нужна ДЕТЕРМИНИРОВАННАЯ точка ровно перед игроком, а не
	// «правильная» проходимость — ставим AlwaysSpawn (без релокации): волк появляется точно в
	// SpawnLoc (≤300 + Z90), сразу попадает в радиус авто-лока (3000) и агрится. Капсула волка
	// (hh=40) при ZOffset=90 висит ~50 над полом и оседает гравитацией — XY при этом не меняется.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWolfCharacter* Wolf = World->SpawnActor<AWolfCharacter>(
		AWolfCharacter::StaticClass(), SpawnLoc, FRotator(0.0f, SpawnRot.Yaw, 0.0f), SpawnParams);

	const float SpawnedDist = Wolf ? FVector::Dist(PawnLoc, Wolf->GetActorLocation()) : -1.0f;
	FQADebug::QA(this, Wolf
		? FString::Printf(TEXT("QA: spawned test wolf %s at dist %.0f"), *Wolf->GetName(), SpawnedDist)
		: TEXT("QA: spawned test wolf FAILED"), /*bScreen=*/true);
}

void AContrarySurvivorPlayerController::OnQAToggleOverlay()
{
	// O: тумблер видимости экранного QA-оверлея.
	FQADebug::bOverlayVisible = !FQADebug::bOverlayVisible;
	FQADebug::QA(this, FString::Printf(TEXT("QA: overlay %s"), FQADebug::bOverlayVisible ? TEXT("on") : TEXT("off")), /*bScreen=*/true);
}

void AContrarySurvivorPlayerController::OnQAForceKillNearest()
{
	// N: мгновенно убить БЛИЖАЙШЕГО врага. Враг = любой Pawn с UStatsComponent, не игрок, живой
	// (тип-агностично — бандит/волк/любой). Урон наносим штатным путём (TakeDamage, как оружие),
	// поэтому отрабатывают override TakeDamage врага -> Stats->ApplyDamage -> HandleDeath ->
	// DropLoot (+ квест-счётчик у волка). С активным force-drop (U) дроп гарантирован.
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		FQADebug::QA(this, TEXT("QA: FORCEKILL skipped - no pawn/world"), /*bScreen=*/true);
		return;
	}

	const FVector PawnLoc = ControlledPawn->GetActorLocation();

	// Выбор цели:
	// 1) ПРИОРИТЕТ — текущая залоченная цель (CurrentTarget: авто-лок или ручной лок),
	//    т.е. то, на что игрок реально наведён. Это и есть «ближайший в радиусе авто-лока».
	// 2) Иначе — РЕАЛЬНО ближайший живой враг по МИНИМУМУ дистанции (тип-агностично).
	//    Раньше N брал просто ближайшего без учёта лока; теперь N детерминированно
	//    добивает залоченную цель (баг QA: добивал дальнего, т.к. лок игнорировался).
	AActor* Target = nullptr;
	if (IsValidTarget(CurrentTarget))
	{
		Target = CurrentTarget;
	}
	else
	{
		float BestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			APawn* Candidate = *It;
			if (!IsValid(Candidate) || Candidate == ControlledPawn)
			{
				continue;
			}
			UStatsComponent* CandStats = Candidate->FindComponentByClass<UStatsComponent>();
			if (!CandStats || CandStats->IsDead())
			{
				continue;
			}
			const float DistSq = FVector::DistSquared(PawnLoc, Candidate->GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Target = Candidate;
			}
		}
	}

	if (!IsValid(Target))
	{
		FQADebug::QA(this, TEXT("QA: FORCEKILL skipped - no living enemy"), /*bScreen=*/true);
		return;
	}

	const FString EnemyName = Target->GetName();
	const float Dist = FVector::Dist(PawnLoc, Target->GetActorLocation());
	const bool bWasLocked = (Target == CurrentTarget);

	// Летальный урон через штатный TakeDamage (как ARangedWeapon: FDamageEvent + инстигатор).
	// Большое число гарантирует смерть даже после брони (ArmorReductionCap всегда пропускает часть).
	FDamageEvent DamageEvent;
	Target->TakeDamage(1000000.0f, DamageEvent, this, ControlledPawn);

	FQADebug::QA(this, FString::Printf(TEXT("QA: FORCEKILL %s (dist %.0f, %s)"),
		*EnemyName, Dist, bWasLocked ? TEXT("locked") : TEXT("nearest")), /*bScreen=*/true);
}

// ---------------------------------------------------------------------------
// QA-харнесс: тест-действия (F1-F4, M = деньги, T = телепорт к торговцу)
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OnToggleDebugCamera()
{
	// F1: переключение свободной debug-камеры. Console-exec "ToggleDebugCamera" роутится в
	// UCheatManager::ToggleDebugCamera (ENGINE_API, UE 5.5) — отвязывает камеру от игрока для
	// свободного облёта (рассмотреть меш/броню/волка/NPC сверху и вблизи), повторно — назад.
	// ConsoleCommand надёжнее прямого вызова: сам найдёт/создаст обработчик cheat-команды.
	ConsoleCommand(TEXT("ToggleDebugCamera"), /*bWriteToLog=*/true);
	UE_LOG(LogQA, Display, TEXT("QA: F1 ToggleDebugCamera"));
}

void AContrarySurvivorPlayerController::OnTestGiveItems()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->GiveTestItems();
		UE_LOG(LogQA, Display, TEXT("QA: F2 GiveTestItems"));
	}
}

void AContrarySurvivorPlayerController::OnTestEquipArmor()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->EquipTestArmor();
		UE_LOG(LogQA, Display, TEXT("QA: F3 EquipTestArmor (test set, default T3)"));
	}
}

void AContrarySurvivorPlayerController::OnTestUnequipArmor()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->UnequipTestArmor();
		UE_LOG(LogQA, Display, TEXT("QA: F4 UnequipTestArmor"));
	}
}

void AContrarySurvivorPlayerController::OnTestGiveMoney()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UStatsComponent* St = PlayerChar->GetStats())
		{
			St->AddMoney(TestMoneyGrant);
			UE_LOG(LogQA, Display, TEXT("QA: +%.0f money, balance %.0f"),
				TestMoneyGrant, St->GetMoney());
		}
	}
}

void AContrarySurvivorPlayerController::OnQATeleportToTrader()
{
	// T: телепортировать игрока вплотную к ближайшему торговцу. «Сборщик» не может подвести
	// игрока к прилавку сверху (волки сбивают), поэтому для верификации купли/продажи нужен
	// мгновенный перенос в радиус взаимодействия. Ставим игрока внутрь InteractTrigger
	// торговца (overlap выставит NearbyTrader) и дополнительно регистрируем торговца напрямую
	// (детерминизм — не зависим от тайминга overlap-события), после чего F9/F10/E работают.
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		UE_LOG(LogQA, Display, TEXT("QA: teleport skipped - no trader"));
		return;
	}

	// A2: ищем ближайшего вендора по интерфейсу (IShopVendor), а не по конкретному классу —
	// торговцем может быть любой актёр, реализующий UShopVendor (сейчас AMasterTrader / BP_Trader).
	AActor* Trader = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector PawnLoc = ControlledPawn->GetActorLocation();
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!IsValid(Candidate) || !Candidate->Implements<UShopVendor>())
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(PawnLoc, Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Trader = Candidate;
		}
	}

	if (!IsValid(Trader))
	{
		UE_LOG(LogQA, Display, TEXT("QA: teleport skipped - no trader"));
		return;
	}

	// Точка рядом с торговцем: смещение по горизонтали (< радиуса триггера, дефолт 220 см),
	// высота игрока сохраняется, чтобы не утопить/не подвесить капсулу.
	const FVector TLoc = Trader->GetActorLocation();
	FVector Dest = TLoc + FVector(120.0f, 0.0f, 0.0f);
	Dest.Z = PawnLoc.Z;

	ControlledPawn->SetActorLocation(Dest, /*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);

	// Гарантированно регистрируем торговца как ближайшего (overlap при телепорте тоже сработает,
	// но прямой вызов убирает зависимость от порядка обновления overlap'ов в этом же кадре).
	SetNearbyTrader(Trader);

	UE_LOG(LogQA, Display, TEXT("QA: teleported to trader at %s"), *Dest.ToCompactString());
}

// ---------------------------------------------------------------------------
// QA-харнесс раунд 3: дублёры UI-действий клавишами (HUD-клики в PIE не доходят
// до тестера из-за захвата мыши). Те же операции, что и по клику, + явный LogQA.
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OnQAUseFirstConsumable()
{
	// F6: использовать ПЕРВЫЙ расходник рюкзака (= клик «использовать»). Подтверждает
	// детерминизм «1 действие на нажатие», эффект еды/воды (+голод/жажда, +HP) и условие
	// авто-регена (Stats->ConsumeFood/DrinkWater зовутся внутри Inv_UseBackpackItem).
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerChar) { return; }

	UInventoryComponent* Inv = PlayerChar->GetInventory();
	UStatsComponent* St = PlayerChar->GetStats();
	if (!Inv || !St) { return; }

	AMasterInventoryItem* Found = nullptr;
	for (AMasterInventoryItem* It : Inv->GetInventoryItems())
	{
		if (It && It->GetItemCategory() == EItemCategory::Consumable && !Inv->IsItemEquipped(It))
		{
			Found = It;
			break;
		}
	}

	if (!Found)
	{
		UE_LOG(LogQA, Display, TEXT("QA: USE skipped - no consumable in backpack"));
		return;
	}

	// Имя берём ДО использования (после Use предмет уничтожается).
	const FString ItemName = Found->ItemName.IsEmpty() ? Found->GetName() : Found->ItemName;

	PlayerChar->Inv_UseBackpackItem(Found);

	int32 ConsumablesLeft = 0;
	for (AMasterInventoryItem* It : Inv->GetInventoryItems())
	{
		if (It && It->GetItemCategory() == EItemCategory::Consumable)
		{
			++ConsumablesLeft;
		}
	}

	UE_LOG(LogQA, Display, TEXT("QA: USE %s -> Hunger=%.0f Thirst=%.0f HP=%.0f, left %d"),
		*ItemName, St->GetHunger(), St->GetThirst(), St->GetHealth(), ConsumablesLeft);
}

void AContrarySurvivorPlayerController::OnQADropFirstItem()
{
	// F7: выбросить ПЕРВЫЙ предмет рюкзака (= клик [X]). Inv_DropItem спавнит мировой пикап
	// у ног и сам пишет QA-строку DROP (тот же путь, что и клик).
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerChar) { return; }

	UInventoryComponent* Inv = PlayerChar->GetInventory();
	if (!Inv) { return; }

	AMasterInventoryItem* First = nullptr;
	for (AMasterInventoryItem* It : Inv->GetInventoryItems())
	{
		if (It)
		{
			First = It;
			break;
		}
	}

	if (!First)
	{
		UE_LOG(LogQA, Display, TEXT("QA: DROP skipped - backpack empty"));
		return;
	}

	PlayerChar->Inv_DropItem(First); // QA-строка DROP пишется внутри
}

void AContrarySurvivorPlayerController::OnQABuyCheapest()
{
	// F9: купить самый дешёвый товар у БЛИЖАЙШЕГО торговца. Без торговца рядом — пропуск с логом.
	// Shop_BuyEntry сам пишет QA-строку BUY (баланс/цена) при успехе.
	if (!IsValid(NearbyTrader.GetObject()))
	{
		UE_LOG(LogQA, Display, TEXT("QA: BUY skipped - no trader near"));
		return;
	}

	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerChar) { return; }

	const TArray<FShopEntry>& Catalog = NearbyTrader->GetCatalog();
	const FShopEntry* Cheapest = nullptr;
	for (const FShopEntry& Entry : Catalog)
	{
		if (!Cheapest || Entry.Price < Cheapest->Price)
		{
			Cheapest = &Entry;
		}
	}

	if (!Cheapest)
	{
		UE_LOG(LogQA, Display, TEXT("QA: BUY skipped - trader catalog empty"));
		return;
	}

	const bool bOk = PlayerChar->Shop_BuyEntry(*Cheapest);
	if (!bOk)
	{
		UE_LOG(LogQA, Display, TEXT("QA: BUY '%s' (%.0f) failed - not enough money / no slot"),
			*Cheapest->DisplayName, Cheapest->Price);
	}
}

void AContrarySurvivorPlayerController::OnQASellFirstItem()
{
	// F10: продать ПЕРВЫЙ предмет рюкзака ближайшему торговцу. Цена выкупа = trader->GetSellValue.
	// Без торговца рядом продавать некому — пропуск с логом (допущение: продажа требует торговца).
	// Shop_SellItem сам пишет QA-строку SELL (баланс/цена).
	if (!IsValid(NearbyTrader.GetObject()))
	{
		UE_LOG(LogQA, Display, TEXT("QA: SELL skipped - no trader near"));
		return;
	}

	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerChar) { return; }

	UInventoryComponent* Inv = PlayerChar->GetInventory();
	if (!Inv) { return; }

	AMasterInventoryItem* First = nullptr;
	for (AMasterInventoryItem* It : Inv->GetInventoryItems())
	{
		if (It)
		{
			First = It;
			break;
		}
	}

	if (!First)
	{
		UE_LOG(LogQA, Display, TEXT("QA: SELL skipped - nothing to sell"));
		return;
	}

	const float SellPrice = NearbyTrader->GetSellValue(First);
	PlayerChar->Shop_SellItem(First, SellPrice); // QA-строка SELL пишется внутри
}

void AContrarySurvivorPlayerController::OnQAClearSave()
{
	// F12: удалить слот сейва 'ContrarySave' (slot/index = дефолты APlayerCharacter).
	// После рестарта PIE BeginPlay не найдёт сейв -> новый игрок -> InitMoney(50) = старт-деньги 50.
	UGameplayStatics::DeleteGameInSlot(TEXT("ContrarySave"), 0);
	UE_LOG(LogQA, Display, TEXT("QA: save 'ContrarySave' cleared - restart PIE for fresh start"));
}

// ---------------------------------------------------------------------------
// QA-харнесс (Фаза 5): квесты/диалог с клавиш (тестер не кликает HUD и не жмёт `~`)
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OnQATeleportToElder()
{
	// Y: телепорт игрока вплотную к ближайшему старосте (как T к торговцу), чтобы сработал
	// NearbyElder и заработали G/H/E. Ставим в радиус InteractTrigger старосты и явно регистрируем.
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		UE_LOG(LogQA, Display, TEXT("QA: teleport skipped - no elder"));
		return;
	}

	AElderNPC* Elder = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector PawnLoc = ControlledPawn->GetActorLocation();
	for (TActorIterator<AElderNPC> It(World); It; ++It)
	{
		AElderNPC* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(PawnLoc, Candidate->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Elder = Candidate;
		}
	}

	if (!IsValid(Elder))
	{
		UE_LOG(LogQA, Display, TEXT("QA: teleport skipped - no elder"));
		return;
	}

	const FVector ELoc = Elder->GetActorLocation();
	FVector Dest = ELoc + FVector(120.0f, 0.0f, 0.0f);
	Dest.Z = PawnLoc.Z;

	ControlledPawn->SetActorLocation(Dest, /*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);
	SetNearbyElder(Elder);

	UE_LOG(LogQA, Display, TEXT("QA: teleported to elder at %s"), *Dest.ToCompactString());
}

void AContrarySurvivorPlayerController::OnQAAcceptQuest()
{
	// G: предложить+принять квест ближайшего старосты (= открыть диалог и нажать [Принять]).
	if (!IsValid(NearbyElder))
	{
		UE_LOG(LogQA, Display, TEXT("QA: accept skipped - no elder near"));
		return;
	}

	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	UQuestComponent* PlayerQuests = PlayerChar ? PlayerChar->GetQuests() : nullptr;
	if (!PlayerQuests)
	{
		return;
	}

	// Выдаём квест по порядку (кв.1, затем кв.2 после сдачи кв.1).
	const FQuest& Offered = NearbyElder->GetQuestForPlayer(PlayerQuests);
	PlayerQuests->OfferQuest(Offered);              // OFFERED (один раз)
	PlayerQuests->AcceptQuest(Offered.QuestId);     // ACCEPTED
}

void AContrarySurvivorPlayerController::OnQATurnInQuest()
{
	// H: сдать выполненный квест ближайшему старосте (= [Сдать]).
	if (!IsValid(NearbyElder))
	{
		UE_LOG(LogQA, Display, TEXT("QA: turn-in skipped - no elder near"));
		return;
	}

	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	UQuestComponent* PlayerQuests = PlayerChar ? PlayerChar->GetQuests() : nullptr;
	if (!PlayerQuests)
	{
		return;
	}

	const FQuest& Offered = NearbyElder->GetQuestForPlayer(PlayerQuests);
	if (!PlayerQuests->TurnInQuest(Offered.QuestId)) // TURNED IN (или skip, если не Completed/нет предметов)
	{
		UE_LOG(LogQA, Display, TEXT("QA: turn-in skipped - quest %s not completed or items missing"),
			*Offered.QuestId.ToString());
	}
}

void AContrarySurvivorPlayerController::OnQACreditWolfKill()
{
	// K: зачесть одно убийство волка в квест (прогресс +1) без поиска живого волка —
	// чтобы прогнать прогресс квеста с клавиатуры. Тег "Wolf" совпадает с тегом квеста старосты.
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	UQuestComponent* PlayerQuests = PlayerChar ? PlayerChar->GetQuests() : nullptr;
	if (!PlayerQuests)
	{
		return;
	}
	PlayerQuests->NotifyKill(FName(TEXT("Wolf"))); // QA: quest progress X/5 (+ COMPLETED)
}

void AContrarySurvivorPlayerController::OnQAGiveWolfHides()
{
	// C: выдать игроку 5 «Шкур волка» в рюкзак (тест сдачи кв.1 без фарма волков). Предметы —
	// квест-категории (AQuestItem), как и реальный дроп волка. Имя ДОЛЖНО совпадать с
	// RequiredItemName кв.1 («Шкура волка»). Tick-синхронизация подхватит прогресс/Completed.
	GiveQuestItems(TEXT("Шкура волка"), 5);
}

void AContrarySurvivorPlayerController::OnQAGiveNotebook()
{
	// X: выдать игроку «Ноутбук» (тест сдачи кв.2). Имя совпадает с RequiredItemName кв.2.
	GiveQuestItems(TEXT("Ноутбук"), 1);
}

void AContrarySurvivorPlayerController::GiveQuestItems(const FString& ItemName, int32 Count)
{
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	UWorld* World = GetWorld();
	if (!PlayerChar || !World)
	{
		UE_LOG(LogQA, Display, TEXT("QA: give quest items skipped - no pawn/world"));
		return;
	}

	UInventoryComponent* Inv = PlayerChar->GetInventory();
	if (!Inv)
	{
		UE_LOG(LogQA, Display, TEXT("QA: give quest items skipped - no inventory"));
		return;
	}

	FActorSpawnParameters Sp;
	Sp.Owner = PlayerChar;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	int32 Given = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		AQuestItem* Item = World->SpawnActor<AQuestItem>(
			AQuestItem::StaticClass(), PlayerChar->GetActorLocation(), PlayerChar->GetActorRotation(), Sp);
		if (!Item)
		{
			continue;
		}
		// Предмет рюкзака — данные, не объект сцены: прячем визуал/коллизию (как GiveTestItems).
		Item->SetActorHiddenInGame(true);
		Item->SetActorEnableCollision(false);
		Item->ItemName = ItemName;
		Inv->AddItem(Item);
		++Given;
	}

	FQADebug::QA(this, FString::Printf(TEXT("QA: gave %d x '%s' (quest item) to backpack"), Given, *ItemName),
		/*bScreen=*/true);
}

#endif // CONTRARY_WITH_QA_CHEATS — конец блока отладочных клавиш

void AContrarySurvivorPlayerController::SetNearbyTrader(TScriptInterface<IShopVendor> Trader)
{
	NearbyTrader = Trader;
}

void AContrarySurvivorPlayerController::ClearNearbyTrader(TScriptInterface<IShopVendor> Trader)
{
	// Сбрасываем, только если уходим именно от текущего торговца.
	if (NearbyTrader == Trader)
	{
		NearbyTrader = nullptr;
		// Ушли от прилавка — закрываем магазин, если был открыт.
		if (bShopOpen)
		{
			CloseShop();
		}
	}
}

void AContrarySurvivorPlayerController::SetNearbyElder(AElderNPC* Elder)
{
	NearbyElder = Elder;

	// Этап F (онбординг): первое приближение к старосте — подсказка (один раз за профиль).
	if (Elder)
	{
		if (UOnboardingComponent* OnboardingComp = GetOnboarding())
		{
			OnboardingComp->TryShowHint(EOnboardingHint::Elder);
		}
	}
}

void AContrarySurvivorPlayerController::ClearNearbyElder(AElderNPC* Elder)
{
	// Сбрасываем, только если уходим именно от текущего старосты.
	if (NearbyElder == Elder)
	{
		NearbyElder = nullptr;
		// Ушли от старосты — закрываем диалог, если был открыт.
		if (bDialogOpen)
		{
			CloseDialog();
		}
	}
}

void AContrarySurvivorPlayerController::OpenDialog(AElderNPC* Elder)
{
	if (!Elder || bDialogOpen)
	{
		return;
	}

	// Предлагаем АКТУАЛЬНЫЙ квест старосты журналу игрока (по порядку: кв.1, затем кв.2 после
	// сдачи кв.1). Идемпотентно; OFFERED логируется один раз.
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UQuestComponent* PlayerQuests = PlayerChar->GetQuests())
		{
			PlayerQuests->OfferQuest(Elder->GetQuestForPlayer(PlayerQuests));
		}

		// Build 1: подарок первой встречи (аптечка) больше НЕ выдаётся при открытии — иначе
		// подсказка «Получена аптечка» всплывала бы до реплики. Теперь аптечку выдаёт
		// скриптовое интро на реплике «Держи, затяни раны» (DialogScreenWidget::AdvanceIntro,
		// действие GiveGift). Признак «уже выдал» по-прежнему в сейве — фарм невозможен.
	}

	bDialogOpen = true;
	bUIClickConsumed = false;

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetDialogOpen(true, Elder);
		// Build 1: игрок дошёл до старосты и заговорил — снимаем интро-задачу и стрелку деревни
		// (дальше ориентиры даёт квест). Идемпотентно, даже если интро уже кончилось.
		CSHUD->SetIntroObjective(FText::GetEmpty());
		CSHUD->SetIntroDirectionTarget(nullptr);
	}

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	UE_LOG(LogQA, Display, TEXT("QA: dialog opened (elder %s)"), *Elder->GetName());
}

void AContrarySurvivorPlayerController::CloseDialog()
{
	if (!bDialogOpen)
	{
		return;
	}
	bDialogOpen = false;
	bUIClickConsumed = false;

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetDialogOpen(false, nullptr);
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = true;
	UE_LOG(LogTemp, Log, TEXT("Dialog CLOSED"));

	// Build 1 (решение Рината 07-24): окно ежедневной награды могло быть отложено до конца
	// интро-диалога (банер портил атмосферу интро) — сообщаем компоненту, что диалог старосты
	// закрылся. Без отложенного окна вызов — тихий no-op.
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UDailyRewardComponent* Daily = PlayerChar->GetDailyReward())
		{
			Daily->NotifyElderDialogClosed();
		}
	}
}

void AContrarySurvivorPlayerController::CloseAllUI()
{
	// Магазин/диалог/обыск трупа/меню паузы: их Close* сами синхронизируют состояние и
	// возвращают режим ввода в Game (early-return, если окно не открыто).
	CloseShop();
	CloseDialog();
	CloseCorpseLoot();
	ClosePauseMenu();

	// Инвентарь: сбрасываем флаг, синхронизируем HUD и режим ввода (как ветка «закрыто» в OnToggleInventory).
	if (bInventoryOpen)
	{
		bInventoryOpen = false;
		bUIClickConsumed = false;
		if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
		{
			CSHUD->SetInventoryOpen(false);
		}
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = true;
		UE_LOG(LogTemp, Log, TEXT("Inventory CLOSED (player death)"));
	}

	UE_LOG(LogQA, Display, TEXT("QA: all UI windows closed on player death"));
}

void AContrarySurvivorPlayerController::OnInteract()
{
	// Не смешиваем с инвентарём (модальные экраны взаимоисключающие).
	if (bInventoryOpen)
	{
		return;
	}

	// Открытый магазин закрываем тем же E.
	if (bShopOpen)
	{
		CloseShop();
		return;
	}

	// Открытый диалог закрываем тем же E.
	if (bDialogOpen)
	{
		CloseDialog();
		return;
	}

	// Открытое окно обыска трупа закрываем тем же E (Build 1.2.1, как магазин).
	if (bCorpseLootOpen)
	{
		CloseCorpseLoot();
		return;
	}

	// Контекстный interact (решение Рината/game-lead): действуем по БЛИЖАЙШЕМУ интерактиву,
	// выбранному в Tick (UpdateNearbyInteractable). Пикап -> подобрать, торговец -> магазин,
	// староста -> диалог.
	switch (CurrentInteractKind)
	{
		case EInteractKind::Pickup:
		{
			APickup* Pickup = Cast<APickup>(CurrentInteractActor);
			APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
			if (Pickup && PlayerChar)
			{
				// Build 1.2.2 (Ринат: «нужно, что бы был BP_Picup с механикой похожей на ту,
				// что я обыскиваю ящик или труп и выбираю что себе положить в инвентарь»):
				// мешок открывает ТО ЖЕ окно обыска, что и труп. Мгновенный забор всего разом
				// остался у пикапов с включённым переключателем на классе.
				if (Pickup->UsesSearchWindow())
				{
					OpenCorpseLoot(Pickup->GetLootContainer());
				}
				else
				{
					const bool bOk = Pickup->Collect(PlayerChar);
					UE_LOG(LogTemp, Log, TEXT("Interact: pickup collect %s"), bOk ? TEXT("OK") : TEXT("FAIL"));
				}
			}
			// Ближайший интерактив пересчитается в следующем Tick.
			break;
		}
		case EInteractKind::Trader:
		{
			// A2: вендор определяется по интерфейсу, не по классу. CurrentInteractActor (AActor*)
			// неявно сворачивается в TScriptInterface<IShopVendor> — интерфейс резолвится кастом.
			if (CurrentInteractActor && CurrentInteractActor->Implements<UShopVendor>())
			{
				OpenShop(CurrentInteractActor);
			}
			break;
		}
		case EInteractKind::Elder:
		{
			if (AElderNPC* Elder = Cast<AElderNPC>(CurrentInteractActor))
			{
				OpenDialog(Elder);
			}
			break;
		}
		case EInteractKind::Corpse:
		{
			// Build 1.2.1 (ТЗ А1): труп врага с лутом — открываем окно обыска.
			if (CurrentInteractActor)
			{
				if (UCorpseLootComponent* Corpse =
					CurrentInteractActor->FindComponentByClass<UCorpseLootComponent>())
				{
					OpenCorpseLoot(Corpse);
				}
			}
			break;
		}
		default:
			UE_LOG(LogTemp, Log, TEXT("Interact: nothing nearby"));
			break;
	}
}

void AContrarySurvivorPlayerController::OpenShop(TScriptInterface<IShopVendor> Trader)
{
	if (!Trader || bShopOpen)
	{
		return;
	}

	bShopOpen = true;
	bUIClickConsumed = false; // свежее состояние edge-клика для нового экрана

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetShopOpen(true, Trader);
	}

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	UE_LOG(LogTemp, Log, TEXT("Shop OPEN (trader %s)"), *GetNameSafe(Trader.GetObject()));
}

void AContrarySurvivorPlayerController::CloseShop()
{
	if (!bShopOpen)
	{
		return;
	}
	bShopOpen = false;
	bUIClickConsumed = false;
	ResetShopTouchState(); // G2: палец мог остаться «в жесте» — не тащить его в закрытый экран

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetShopOpen(false, nullptr);
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = true;
	UE_LOG(LogTemp, Log, TEXT("Shop CLOSED"));
}

void AContrarySurvivorPlayerController::OpenCorpseLoot(UCorpseLootComponent* Corpse)
{
	// Build 1.2.1 (ТЗ А1): окно обыска трупа — модалка по образцу OpenShop.
	if (!Corpse || bCorpseLootOpen)
	{
		return;
	}

	bCorpseLootOpen = true;
	bUIClickConsumed = false;

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetCorpseLootOpen(true, Corpse);
	}

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	UE_LOG(LogQA, Display, TEXT("QA: CORPSE loot window OPEN ('%s')"),
		*GetNameSafe(Corpse->GetOwner()));
}

void AContrarySurvivorPlayerController::CloseCorpseLoot()
{
	if (!bCorpseLootOpen)
	{
		return;
	}
	bCorpseLootOpen = false;
	bUIClickConsumed = false;

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetCorpseLootOpen(false, nullptr);
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = true;
	UE_LOG(LogQA, Display, TEXT("QA: CORPSE loot window CLOSED"));
}

void AContrarySurvivorPlayerController::OnToggleInventory()
{
	bInventoryOpen = !bInventoryOpen;
	bUIClickConsumed = false; // свежее состояние edge-клика на смене экрана (BUG1)

	// Синхронизируем экран инвентаря на HUD (immediate-mode отрисовка).
	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetInventoryOpen(bInventoryOpen);
	}

	// Режим ввода: открыто -> GameAndUI (курсор виден, клики читаются как UI-клики инвентаря,
	// игровые экшены тоже доходят — гейтятся флагом bInventoryOpen в Fire/Move). Закрыто -> GameOnly.
	if (bInventoryOpen)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		bShowMouseCursor = true;
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = true; // курсор нужен и в игре (клик-таргетинг)
	}

	UE_LOG(LogTemp, Log, TEXT("Inventory %s"), bInventoryOpen ? TEXT("OPEN") : TEXT("CLOSED"));

	// Этап F (онбординг): первое открытие инвентаря — подсказка про слоты брони/защиту.
	if (bInventoryOpen)
	{
		if (UOnboardingComponent* OnboardingComp = GetOnboarding())
		{
			OnboardingComp->TryShowHint(EOnboardingHint::Inventory);
		}
	}
}

void AContrarySurvivorPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Интро (Build 1): один раз решаем и запускаем (когда появилась пешка), затем ведём по фазам.
	if (!bIntroChecked)
	{
		MaybeStartIntro();
	}
	if (IntroPhase != EIntroPhase::None)
	{
		UpdateIntro(DeltaTime);
	}

	// Поддерживаем авто-лок на ближайшей живой цели (по умолчанию и после смерти текущей).
	UpdateAutoTarget();

	// Поддерживаем ближайший контекстный интерактив (E): пикап/торговец (BUG3).
	UpdateNearbyInteractable();

	// Фаза 5: синхронизируем ITEM-прогресс квестов (Collect/Deliver) с содержимым рюкзака —
	// число «Шкур волка»/«Ноутбук» в инвентаре. SyncInventoryQuests меняет состояние/шлёт
	// событие ТОЛЬКО при реальном изменении (без спама на каждый тик).
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UQuestComponent* PlayerQuests = PlayerChar->GetQuests())
		{
			PlayerQuests->SyncInventoryQuests(PlayerChar->GetInventory());
		}
	}

	// QA-харнесс: логируем СМЕНУ залоченной цели один раз (не каждый тик), чтобы тестер
	// видел по логу, на кого сейчас наведён лок (авто-ближайший или ручной фокус).
	if (CurrentTarget != LastLoggedTarget)
	{
		if (CurrentTarget)
		{
			UE_LOG(LogQA, Display, TEXT("QA: lock target -> %s (%s)"),
				*CurrentTarget->GetName(), bManualLock ? TEXT("manual") : TEXT("auto"));
		}
		else
		{
			UE_LOG(LogQA, Display, TEXT("QA: lock target cleared"));
		}
		LastLoggedTarget = CurrentTarget;
	}
}

// ---------------------------------------------------------------------------
// Интро (Build 1, ТЗ издателя раздел 2)
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::MaybeStartIntro()
{
	// Ждём появления пешки-игрока (possess может произойти на пару кадров позже BeginPlay).
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerChar)
	{
		return; // попробуем в следующем кадре
	}
	bIntroChecked = true; // решение принимаем ровно один раз

	// Волна «Главное меню» (ADR-062, спека glavnoe-menu-spec.md): самый первый запуск после
	// установки идёт сразу во вступление, со второго и всех последующих запусков игра
	// открывается главным меню. Признак запуска живёт в памяти на установку (слот
	// ContraryAnalytics): «Новая игра» и стирание игрового сейва его НЕ сбрасывают. Сюда
	// попадаем ровно один раз за запуск игры (после смерти BeginPlay контроллера не
	// вызывается повторно — пешка та же), так что пометка запуска не задваивается.
	const bool bHasSave = PlayerChar->HasSaveGame();
	bool bLaunchedBefore = false;
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		bLaunchedBefore = Analytics->MarkLaunchAndCheckWasLaunchedBefore();
	}
	if (ShouldShowMainMenuOnLaunch(bLaunchedBefore, bHasSave))
	{
		// Выбор («Продолжить»/«Новая игра»/…) — за меню; интро (если будет) запустит
		// HandleStartScreenNewGame через StartNewGameFlow.
		OpenStartScreen();
		return;
	}

	// Самый первый запуск после установки — без меню, сразу обычная новая игра со вступлением.
	StartNewGameFlow();
}

void AContrarySurvivorPlayerController::StartNewGameFlow()
{
	if (!bEnableIntro)
	{
		return; // интро выключено (отладка команды) — сразу обычная игра
	}

	// Б3, замечание 9 ревизии: интро играет ТОЛЬКО при новой игре (при «Продолжить» — не
	// запускается вовсе). Прежнего варианта «повторный заход = интро с hold-to-skip» больше нет —
	// повторный показ каждого запуска и был жалобой издателя; теперь новая игра тут ровно одна
	// (сейва не было, либо игрок сам стёр его на стартовом экране), интро всегда полное.
	StartIntro(/*bSkippable=*/false);
}

void AContrarySurvivorPlayerController::StartIntro(bool bSkippable)
{
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerChar)
	{
		return;
	}

	bIntroSkippable = bSkippable;
	bIntroSkipped = false;
	IntroElapsed = 0.0f;
	IntroAutoWalkElapsed = 0.0f;
	IntroSkipHeld = 0.0f;
	IntroPhase = EIntroPhase::Line1;

	FindIntroVillageTarget(PlayerChar);

	// Чёрный экран + тёмный грейд + блок ввода движения. Курсор прячем — чистый кадр кинематографа.
	bIntroInputLocked = true;
	bShowMouseCursor = false;
	PlayerChar->SetIntroGradeAlpha(0.0f);

	if (!IntroWidget)
	{
		// Слот назначен (ADR-048) — окно из WBP, правится в дизайнере; пусто — кодовое дерево.
		IntroWidget = CreateWidget<UIntroScreenWidget>(this,
			IntroScreenWidgetClass ? IntroScreenWidgetClass.Get() : UIntroScreenWidget::StaticClass());
	}
	if (IntroWidget)
	{
		IntroWidget->SetBackgroundAlpha(1.0f);
		IntroWidget->SetTextAlpha(0.0f);
		IntroWidget->SetLineText(IntroLine1);
		IntroWidget->SetSkipHint(IntroSkipHintText, bIntroSkippable);
		if (!IntroWidget->IsInViewport())
		{
			IntroWidget->AddToViewport(/*ZOrder=*/50); // выше HUD/модалок
		}
	}

	// Пока идёт интро — не даём онбордингу показать подсказку движения по авто-таймеру: покажем
	// её сами ровно при передаче управления (иначе всплыла бы на чёрном экране и «сгорела»).
	if (UOnboardingComponent* Onb = PlayerChar->GetOnboarding())
	{
		Onb->CancelPendingMovementHint();
	}

	UE_LOG(LogQA, Display, TEXT("QA: intro started (%s)"), bIntroSkippable ? TEXT("repeat/skippable") : TEXT("new game"));
}

void AContrarySurvivorPlayerController::FindIntroVillageTarget(const APlayerCharacter* PlayerChar)
{
	// Цель-деревня: актор с тегом VillageMarkerTag; фолбэк — ближайший староста (он в деревне).
	IntroVillageActor = nullptr;
	bIntroHasVillage = false;
	if (UWorld* World = GetWorld())
	{
		if (!VillageMarkerTag.IsNone())
		{
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->ActorHasTag(VillageMarkerTag))
				{
					IntroVillageActor = *It;
					break;
				}
			}
		}
		if (!IntroVillageActor && PlayerChar)
		{
			float BestSq = TNumericLimits<float>::Max();
			const FVector Loc = PlayerChar->GetActorLocation();
			for (TActorIterator<AElderNPC> It(World); It; ++It)
			{
				const float DSq = FVector::DistSquared(Loc, It->GetActorLocation());
				if (DSq < BestSq) { BestSq = DSq; IntroVillageActor = *It; }
			}
		}
	}
	if (IntroVillageActor && PlayerChar)
	{
		IntroVillageLocation = IntroVillageActor->GetActorLocation();
		bIntroHasVillage = true;
		FVector ToV = IntroVillageLocation - PlayerChar->GetActorLocation();
		ToV.Z = 0.0f;
		IntroInitialDistance = FMath::Max(1.0f, ToV.Size());
	}
}

bool AContrarySurvivorPlayerController::ShouldResumeIntroObjectiveAfterContinue(
	bool bIntroEnabled, int32 RestoredQuestCount)
{
	// Интро-этап закончен ровно тогда, когда игрок дошёл до старосты и взял квест: журнал
	// перестаёт быть пустым (и таким сохраняется — квесты из журнала не удаляются). Дальше
	// ориентиры дают трекер квестов и метка цели квеста — они рисуются из восстановленного
	// журнала сами, интро-баннер не нужен.
	return bIntroEnabled && RestoredQuestCount == 0;
}

void AContrarySurvivorPlayerController::ResumeIntroObjectiveAfterContinue()
{
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerChar)
	{
		return;
	}

	const UQuestComponent* PlayerQuests = PlayerChar->GetQuests();
	const int32 RestoredQuestCount = PlayerQuests ? PlayerQuests->GetQuests().Num() : 0;
	if (!ShouldResumeIntroObjectiveAfterContinue(bEnableIntro, RestoredQuestCount))
	{
		return;
	}

	FindIntroVillageTarget(PlayerChar);

	// Фаза HandOff = «управление у игрока, ждём входа в деревню»: чёрный экран/блок ввода
	// не включаются, а переход задачи на «найти старосту» у околицы сделает штатный UpdateIntro.
	IntroPhase = EIntroPhase::HandOff;

	if (AContrarySurvivorHUD* H = GetHUD<AContrarySurvivorHUD>())
	{
		H->SetIntroObjective(IntroObjectiveGoToVillage);
		H->SetIntroDirectionTarget(IntroVillageActor);
	}

	UE_LOG(LogQA, Display, TEXT("QA: CONTINUE mid-intro - objective banner and village arrow restored (village target %s)"),
		IntroVillageActor ? *IntroVillageActor->GetName() : TEXT("none"));
}

void AContrarySurvivorPlayerController::UpdateIntro(float DeltaTime)
{
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	if (!PlayerChar)
	{
		return;
	}

	IntroElapsed += DeltaTime;

	// Hold-to-skip (только повторные заходы, до передачи управления): копим удержание, сброс при отпускании.
	if (bIntroSkippable && IntroPhase != EIntroPhase::HandOff)
	{
		if (IsIntroSkipHeld())
		{
			IntroSkipHeld += DeltaTime;
			if (IntroSkipHeld >= IntroSkipHoldTime)
			{
				bIntroSkipped = true;
				PlayerChar->SetIntroGradeAlpha(1.0f); // при пропуске — сразу нормальный кадр
				IntroHandOverControl();
				return;
			}
		}
		else
		{
			IntroSkipHeld = 0.0f;
		}
	}

	switch (IntroPhase)
	{
		case EIntroPhase::Line1:
		case EIntroPhase::Line2:
		{
			const bool bLine1 = (IntroPhase == EIntroPhase::Line1);
			const float PhaseStart = bLine1 ? 0.0f : IntroLineDuration;
			const float LocalT = IntroElapsed - PhaseStart;

			// Треугольная альфа строки: проступает в первой трети, держится, гаснет в последней.
			const float FadeT = FMath::Max(0.01f, IntroLineDuration / 3.0f);
			float A = 1.0f;
			if (LocalT < FadeT)                          { A = LocalT / FadeT; }
			else if (LocalT > IntroLineDuration - FadeT) { A = (IntroLineDuration - LocalT) / FadeT; }
			A = FMath::Clamp(A, 0.0f, 1.0f);

			if (IntroWidget)
			{
				IntroWidget->SetLineText(bLine1 ? IntroLine1 : IntroLine2);
				IntroWidget->SetTextAlpha(A);
				IntroWidget->SetBackgroundAlpha(1.0f);
			}

			if (LocalT >= IntroLineDuration)
			{
				IntroPhase = bLine1 ? EIntroPhase::Line2 : EIntroPhase::Reveal;
				if (IntroPhase == EIntroPhase::Reveal)
				{
					// Мир начинает проявляться — ставим первую задачу и стрелку на деревню.
					if (AContrarySurvivorHUD* H = GetHUD<AContrarySurvivorHUD>())
					{
						H->SetIntroObjective(IntroObjectiveGoToVillage);
						H->SetIntroDirectionTarget(IntroVillageActor);
					}
				}
			}
			break;
		}
		case EIntroPhase::Reveal:
		case EIntroPhase::AutoWalk:
		{
			IntroAutoWalkElapsed += DeltaTime;

			// Проявление мира ПО ВРЕМЕНИ: чёрный фон и экспозиция-грейд светлеют синхронно за
			// IntroRevealDuration — от 0 (черно/тёмно) к 1 (норма). НЕ по дистанции до деревни:
			// иначе на старте reveal игрок далеко → Progress≈0 → экспозиция в самом тёмном → чёрный кадр.
			const float RevealProgress = FMath::Clamp(
				IntroAutoWalkElapsed / FMath::Max(0.01f, IntroRevealDuration), 0.0f, 1.0f);
			if (IntroWidget)
			{
				IntroWidget->SetTextAlpha(0.0f);
				IntroWidget->SetBackgroundAlpha(1.0f - RevealProgress);
			}
			PlayerChar->SetIntroGradeAlpha(RevealProgress);

			// Авто-подход: персонаж сам, хромая, идёт к деревне (хромота от низкого HP уже активна).
			if (bIntroHasVillage)
			{
				FVector Dir = IntroVillageLocation - PlayerChar->GetActorLocation();
				Dir.Z = 0.0f;
				if (!Dir.IsNearlyZero())
				{
					if (IsMoveInputIgnored()) { ResetIgnoreMoveInput(); }
					PlayerChar->AddMovementInput(Dir.GetSafeNormal(), 1.0f);
				}
			}

			if (IntroPhase == EIntroPhase::Reveal && IntroAutoWalkElapsed >= IntroRevealDuration)
			{
				IntroPhase = EIntroPhase::AutoWalk;
			}

			// Передача управления: не раньше, чем мир полностью проявился И прошёл авто-подход.
			if (IntroAutoWalkElapsed >= FMath::Max(IntroAutoApproachDuration, IntroRevealDuration))
			{
				IntroHandOverControl();
			}
			break;
		}
		case EIntroPhase::HandOff:
		{
			// Управление у игрока. Экспозиция уже доведена до нормы (1.0) во время reveal по времени —
			// НЕ затемняем обратно по дистанции. Просто ждём входа в деревню, чтобы сменить задачу.
			if (bIntroHasVillage)
			{
				FVector ToV = IntroVillageLocation - PlayerChar->GetActorLocation();
				ToV.Z = 0.0f;
				if (ToV.Size() <= IntroSafeZoneRadius)
				{
					EndIntro();
				}
			}
			else
			{
				// Нет цели-деревни (не размечена и старосты нет) — просто завершаем интро.
				EndIntro();
			}
			break;
		}
		default:
			break;
	}
}

void AContrarySurvivorPlayerController::IntroHandOverControl()
{
	bIntroInputLocked = false;
	bShowMouseCursor = true; // управление у игрока — курсор для выбора цели снова нужен
	IntroPhase = EIntroPhase::HandOff;

	// Чёрный экран отработал — убираем виджет интро (при пропуске это мгновенно открывает мир).
	if (IntroWidget && IntroWidget->IsInViewport())
	{
		IntroWidget->RemoveFromParent();
	}

	// Задача и стрелка на деревню (идемпотентно: при пропуске мир не «проявлялся», ставим сейчас).
	if (AContrarySurvivorHUD* H = GetHUD<AContrarySurvivorHUD>())
	{
		H->SetIntroObjective(IntroObjectiveGoToVillage);
		H->SetIntroDirectionTarget(IntroVillageActor);
	}

	// Подсказка движения (левый стик) — ровно в момент передачи управления (ТЗ раздел 2).
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UOnboardingComponent* Onb = PlayerChar->GetOnboarding())
		{
			Onb->TryShowHint(EOnboardingHint::Movement);
		}
	}

	UE_LOG(LogQA, Display, TEXT("QA: intro control handed to player%s"), bIntroSkipped ? TEXT(" (skipped)") : TEXT(""));
}

void AContrarySurvivorPlayerController::EndIntro()
{
	IntroPhase = EIntroPhase::None;

	// Грейд в норму (на случай, если арка не дотянула до 1 у околицы).
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->SetIntroGradeAlpha(1.0f);
	}

	// Задача меняется на «найти старосту», и маркер СИНХРОННО переезжает с деревни на старосту
	// (Build 1.2, задача Рината 07-31 «маркеры по необходимости»: дошёл до деревни — маркер
	// деревни гаснет, загорается маркер старосты). Старосты на карте нет — оставляем прежнюю
	// цель-деревню. Снимаются задача и маркер при открытии диалога (OpenDialog).
	if (AContrarySurvivorHUD* H = GetHUD<AContrarySurvivorHUD>())
	{
		H->SetIntroObjective(IntroObjectiveFindElder);

		AActor* NearestElder = nullptr;
		if (UWorld* World = GetWorld())
		{
			float BestSq = TNumericLimits<float>::Max();
			const FVector From = GetPawn() ? GetPawn()->GetActorLocation() : IntroVillageLocation;
			for (TActorIterator<AElderNPC> It(World); It; ++It)
			{
				const float DSq = FVector::DistSquared(From, It->GetActorLocation());
				if (DSq < BestSq)
				{
					BestSq = DSq;
					NearestElder = *It;
				}
			}
		}
		if (NearestElder)
		{
			H->SetIntroDirectionTarget(NearestElder);
		}
	}

	UE_LOG(LogQA, Display, TEXT("QA: intro ended (entered village), objective -> find elder"));
}

bool AContrarySurvivorPlayerController::IsIntroSkipHeld() const
{
	// Пропуск удержанием: любое «действие» — ЛКМ / пробел / Enter / кнопка геймпада / палец.
	// Тач-клавиши в EKeys — массив TouchKeys[ETouchIndex] (EKeys::Touch1 не существует).
	return IsInputKeyDown(EKeys::LeftMouseButton)
		|| IsInputKeyDown(EKeys::SpaceBar)
		|| IsInputKeyDown(EKeys::Enter)
		|| IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom)
		|| IsInputKeyDown(EKeys::TouchKeys[ETouchIndex::Touch1]);
}

void AContrarySurvivorPlayerController::UpdateNearbyInteractable()
{
	CurrentInteractActor = nullptr;
	CurrentInteractKind = EInteractKind::None;

	// Пока открыт модальный экран (вкл. экран смерти и стартовый экран Б3) — подсказку не предлагаем.
	if (bInventoryOpen || bShopOpen || bDialogOpen || bCorpseLootOpen || bDeathScreen || bStartScreenOpen)
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		return;
	}

	const FVector Loc = ControlledPawn->GetActorLocation();
	float BestDistSq = TNumericLimits<float>::Max();

	// Торговец: проксимити уже задана его overlap-триггером (NearbyTrader). Вендор хранится как
	// интерфейс (A2) — для дистанции/идентичности берём его UObject как актёра (GetObject).
	if (AActor* TraderActor = Cast<AActor>(NearbyTrader.GetObject()))
	{
		BestDistSq = FVector::DistSquared(Loc, TraderActor->GetActorLocation());
		CurrentInteractActor = TraderActor;
		CurrentInteractKind = EInteractKind::Trader;
	}

	// Староста: проксимити задана его overlap-триггером (NearbyElder). Если ближе торговца — он.
	if (IsValid(NearbyElder))
	{
		const float DistSq = FVector::DistSquared(Loc, NearbyElder->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			CurrentInteractActor = NearbyElder;
			CurrentInteractKind = EInteractKind::Elder;
		}
	}

	// Пикапы: ближайший непустой пикап в InteractRange. Если ближе торговца — он и выигрывает.
	const float RangeSq = InteractRange * InteractRange;
	for (TActorIterator<APickup> It(World); It; ++It)
	{
		APickup* Pickup = *It;
		if (!IsValid(Pickup) || !Pickup->HasLoot())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Loc, Pickup->GetActorLocation());
		if (DistSq <= RangeSq && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			CurrentInteractActor = Pickup;
			CurrentInteractKind = EInteractKind::Pickup;
		}
	}

	// Трупы врагов с лутом (Build 1.2.1, ТЗ А1): реестр обыскиваемых вместо перебора
	// всех акторов мира. Полностью обысканный труп (HasLoot()=false) подсказку не даёт,
	// хотя лежит до таймера. Дистанция — та же InteractRange, конкуренция честная.
	for (const TWeakObjectPtr<UCorpseLootComponent>& Ptr : UCorpseLootComponent::GetSearchableCorpses())
	{
		UCorpseLootComponent* Corpse = Ptr.Get();
		AActor* CorpseOwner = Corpse ? Corpse->GetOwner() : nullptr;
		if (!Corpse || !IsValid(CorpseOwner) || Corpse->GetWorld() != World || !Corpse->HasLoot())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Loc, CorpseOwner->GetActorLocation());
		if (DistSq <= RangeSq && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			CurrentInteractActor = CorpseOwner;
			CurrentInteractKind = EInteractKind::Corpse;
		}
	}

	// Этап F (онбординг): первый доступный подбор — подсказка «Нажми E...». Зовётся каждый
	// тик, но TryShowHint проверяет флаг в памяти и после первого показа — no-op.
	if (CurrentInteractKind == EInteractKind::Pickup)
	{
		if (UOnboardingComponent* OnboardingComp = GetOnboarding())
		{
			OnboardingComp->TryShowHint(EOnboardingHint::Pickup);
		}
	}
}

bool AContrarySurvivorPlayerController::HasInteractPrompt() const
{
	return CurrentInteractKind != EInteractKind::None && IsValid(CurrentInteractActor);
}

FText AContrarySurvivorPlayerController::GetInteractActionText(EInteractKind Kind, bool bPickupUsesSearchWindow) const
{
	// Тексты — EditAnywhere-поля (директива Рината 07-18); здесь только выбор нужного.
	switch (Kind)
	{
		// Build 1.2.2: мешок с окном обыска подписывается как труп — «Обыскать», потому что
		// действие теперь одно и то же. Мгновенный подбор остаётся «Подобрать».
		case EInteractKind::Pickup: return bPickupUsesSearchWindow ? InteractPromptCorpseAction : InteractPromptPickupAction;
		case EInteractKind::Trader: return InteractPromptTraderAction;
		case EInteractKind::Elder:  return InteractPromptElderAction;
		case EInteractKind::Corpse: return InteractPromptCorpseAction;
		default:                    return FText::GetEmpty();
	}
}

FText AContrarySurvivorPlayerController::GetInteractHowText() const
{
	// Тач-слой показан — называем экранную кнопку, иначе клавишу компьютера.
	return HasTouchLayer() ? GetInteractTouchButtonName() : GetInteractKeyName();
}

FText AContrarySurvivorPlayerController::FormatInteractPrompt(const FText& Format, const FText& ActionText, const FText& HowText)
{
	// Действия нет — подсказки нет вовсе (иначе на экран уехало бы одинокое тире).
	if (ActionText.IsEmpty())
	{
		return FText::GetEmpty();
	}
	// Способ не задан — остаётся голое действие, как было до 08-06 (не тупик, а откат).
	if (HowText.IsEmpty())
	{
		return ActionText;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Action"), ActionText);
	Args.Add(TEXT("How"), HowText);
	return FText::Format(Format, Args);
}

FText AContrarySurvivorPlayerController::GetInteractPromptDisplayText() const
{
	const APickup* NearPickup = (CurrentInteractKind == EInteractKind::Pickup)
		? Cast<APickup>(CurrentInteractActor) : nullptr;
	const bool bSearchWindow = NearPickup && NearPickup->UsesSearchWindow();

	return FormatInteractPrompt(InteractPromptFormat,
		GetInteractActionText(CurrentInteractKind, bSearchWindow), GetInteractHowText());
}

FString AContrarySurvivorPlayerController::GetInteractPromptText() const
{
	// Старый Canvas-путь HUD рисует строкой (ADR-048: путь выпиливается) — отдаём ему
	// тот же текст, снятый в строку.
	return GetInteractPromptDisplayText().ToString();
}

void AContrarySurvivorPlayerController::OnFireReleased(const FInputActionValue& Value)
{
	// BUG1: клик отпущен — следующий клик снова считается новым UI-действием.
	bUIClickConsumed = false;
}

void AContrarySurvivorPlayerController::UpdateAutoTarget()
{
	// Вариант A (решение Рината).
	// 1) Если текущая цель умерла/исчезла — снимаем ручной фокус и возвращаемся в авто.
	if (!IsValidTarget(CurrentTarget))
	{
		bManualLock = false;
		CurrentTarget = nullptr;
	}

	// 2) Авто-режим (ручного фокуса нет): КАЖДЫЙ тик лочим БЛИЖАЙШУЮ живую цель.
	//    Появилась ближе — лок динамически перекидывается на неё.
	if (!bManualLock)
	{
		CurrentTarget = FindNearestLivingTarget();
	}
	// 3) Ручной фокус (bManualLock && цель жива) — держим текущую, авто НЕ перекидывает.
}

void AContrarySurvivorPlayerController::OnSwitchWeapon()
{
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->SwitchWeapon();
	}
}

void AContrarySurvivorPlayerController::Move(const FInputActionValue& Value)
{
	// Пока открыт инвентарь/магазин/диалог/обыск трупа/экран смерти/меню паузы/стартовый экран
	// Б3 ИЛИ идёт авто-подход интро — движение игрока подавлено. Гейт общий для WASD и тач-стика
	// (инжекция стика идёт тем же MoveAction). Во время интро персонажа ведёт авто-подход.
	if (bInventoryOpen || bShopOpen || bDialogOpen || bCorpseLootOpen || bDeathScreen || bPauseMenuOpen || bStartScreenOpen || bIntroInputLocked)
	{
		return;
	}

	FVector2D MovementVector = Value.Get<FVector2D>();

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	// БАЗИС ДВИЖЕНИЯ (BugReport12 Этап1, ФИКС). Камера — ФИКСИРОВАННАЯ изометрия (pitch −60, без
	// вращения игроком), поэтому направление движения НЕ должно зависеть от камеры/control rotation.
	// БЫЛО: forward/right строились из GetCameraRotation().Yaw — единственная внешняя зависимость,
	// способная вернуть вырожденный/нулевой горизонтальный вектор (PIE-лог: in=(0,1), но accel/vel=0
	// при mode=Walking/onGround/maxWS=600 → в CMC уходил нулевой MoveDir). СТАЛО: строим базис от
	// ПОСТОЯННОГО горизонтального yaw (MovementBasisYaw, дефолт 90 = совпадает с CameraBoomRotation.Yaw,
	// экранное «вверх» сохраняется), forward/right строго в плоскости Z=0, нормализованы — ненулевой
	// горизонтальный вектор гарантирован независимо от pitch/состояния камеры.
	const FRotator BasisRot(0.0f, MovementBasisYaw, 0.0f);
	FVector Forward = BasisRot.Vector();                             // forward (ось X yaw-базиса)
	FVector Right   = BasisRot.RotateVector(FVector::RightVector);   // right (ось Y yaw-базиса)
	Forward.Z = 0.0f;
	Right.Z   = 0.0f;
	Forward = Forward.GetSafeNormal();
	Right   = Right.GetSafeNormal();
	if (Forward.IsNearlyZero()) { Forward = FVector::ForwardVector; } // фолбэк (теоретически не нужен)
	if (Right.IsNearlyZero())   { Right   = FVector::RightVector; }

	const FVector MoveDir = (Forward * MovementVector.Y) + (Right * MovementVector.X);

	// === BugReport12 Этап1 (ДОБИВ): ввод движения ИГНОРИРУЕТСЯ ============================
	// PIE-лог показал: MoveDir валиден (0,1,0), mode=Walking, onGround=1, maxWS=600 — но vel2D=0.
	// Значит AddMovementInput(валидный вектор) копит НОЛЬ → IsMoveInputIgnored()==true: где-то
	// остался SetIgnoreMoveInput(true) без парного сброса (god/lock/телепорт/debug-камера/BP).
	// APawn::Internal_AddMovementInput отбрасывает ввод при IsMoveInputIgnored() && !bForce →
	// ControlInputVector=0 → CMC не ускоряется. СНИМАЕМ игнор перед применением ввода (демке
	// move-lock не нужен — модальные экраны уже гейтятся ранним return по флагам UI выше).
	if (IsMoveInputIgnored())
	{
		ResetIgnoreMoveInput(); // обнуляет счётчик IgnoreMoveInput (надёжнее, чем SetIgnoreMoveInput(false))
	}

	ControlledPawn->AddMovementInput(MoveDir, 1.0f);
}

void AContrarySurvivorPlayerController::Sprint(const FInputActionValue& Value)
{
	APawn* ControlledPawn = GetPawn();
	if (auto* PlayerChar = Cast<AMasterHumanoidCharacter>(ControlledPawn))
	{
		PlayerChar->SetSprint(Value.Get<bool>());
	}
}

void AContrarySurvivorPlayerController::Fire(const FInputActionValue& Value)
{
	// Меню паузы/стартовый экран Б3: клики обрабатывают кнопки виджета, не стрельба. При паузе
	// Enhanced Input и так молчит (bTriggerWhenPaused=false) — гейт на случай кадров до/после SetPause.
	if (bPauseMenuOpen || bStartScreenOpen)
	{
		return;
	}

	// Build 1: во время кинематографа интро (до передачи управления) не стреляем — та же ЛКМ
	// служит для hold-to-skip, стрельба в пустоту при пропуске была бы лишней.
	if (bIntroInputLocked)
	{
		return;
	}

	// #26: на экране смерти клик уходит в кнопку «Возродиться» (не в стрельбу). EDGE-схема.
	if (bDeathScreen)
	{
		if (!bUIClickConsumed)
		{
			bUIClickConsumed = true;
			if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
			{
				float MX = 0.0f, MY = 0.0f;
				if (GetMousePosition(MX, MY) && CSHUD->HandleDeathScreenClick(FVector2D(MX, MY)))
				{
					if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
					{
						PlayerChar->Respawn();
					}
				}
			}
		}
		return;
	}

	// Если открыт инвентарь — клик уходит в UI инвентаря (надеть/снять/использовать/выбросить),
	// НЕ в стрельбу/таргетинг. BUG1: обрабатываем как EDGE — одно действие на нажатие. Пока кнопка
	// зажата (Triggered летит каждый кадр), повторно НЕ реагируем; флаг снимается на отпускании.
	// Это убирает «прокликивание» всего списка из-за reflow после использования предмета.
	if (bInventoryOpen)
	{
		if (!bUIClickConsumed)
		{
			bUIClickConsumed = true;
			if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
			{
				float MX = 0.0f, MY = 0.0f;
				if (GetMousePosition(MX, MY))
				{
					CSHUD->HandleInventoryClick(FVector2D(MX, MY));
				}
			}
		}
		return;
	}

	// Если открыт магазин — клик уходит в UI магазина (купить/продать/закрыть), не в стрельбу.
	// Та же EDGE-схема (BUG1): один клик = одна покупка/продажа, без авто-повтора при зажатии.
	if (bShopOpen)
	{
		if (!bUIClickConsumed)
		{
			bUIClickConsumed = true;
			if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
			{
				float MX = 0.0f, MY = 0.0f;
				if (GetMousePosition(MX, MY))
				{
					CSHUD->HandleShopClick(FVector2D(MX, MY));
				}
			}
		}
		return;
	}

	// Окно обыска трупа (Build 1.2.1): клики обрабатывают Slate-кнопки самого окна
	// (UMG-виджет), стрельбу глушим целиком — как у прочих модалок.
	if (bCorpseLootOpen)
	{
		return;
	}

	// Если открыт диалог — клик уходит в UI диалога (выбор ответа), не в стрельбу. EDGE-схема.
	if (bDialogOpen)
	{
		if (!bUIClickConsumed)
		{
			bUIClickConsumed = true;
			if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
			{
				float MX = 0.0f, MY = 0.0f;
				if (GetMousePosition(MX, MY))
				{
					CSHUD->HandleDialogClick(FVector2D(MX, MY));
				}
			}
		}
		return;
	}

	// Клик/тап = ручной выбор цели (вариант A): тап по врагу -> ручной фокус на нём;
	// тап по пустому месту -> снять ручной фокус и взять авто-ближайшую. TrySelectTarget
	// уже выставляет CurrentTarget/bManualLock в обоих случаях.
	TrySelectTarget();

	// Страховка: если после выбора цель всё же невалидна — авто-ближайшая.
	if (!IsValidTarget(CurrentTarget))
	{
		bManualLock = false;
		CurrentTarget = FindNearestLivingTarget();
	}

	AMasterHumanoidCharacter* PlayerChar = Cast<AMasterHumanoidCharacter>(GetPawn());
	if (!PlayerChar) return;

	// Синхронизируем цель оружия. Передаём CurrentTarget (может быть nullptr) — это ВАЖНО:
	// SetTarget(nullptr) сбрасывает LockedTarget оружия, иначе Fire() оружия падает обратно
	// на устаревшую (мёртвую) цель (FiringTarget = Target ? Target : LockedTarget) — был БАГ.
	if (ARangedWeapon* Weapon = Cast<ARangedWeapon>(PlayerChar->GetCurrentWeapon()))
	{
		Weapon->SetTarget(CurrentTarget);
	}

	PlayerChar->FireCurrentWeapon(CurrentTarget);
}

void AContrarySurvivorPlayerController::Reload(const FInputActionValue& Value)
{
	AMasterHumanoidCharacter* PlayerChar = Cast<AMasterHumanoidCharacter>(GetPawn());
	if (PlayerChar)
	{
		PlayerChar->ReloadCurrentWeapon();
	}
}

void AContrarySurvivorPlayerController::TrySelectTarget()
{
	AActor* HitActor = GetActorUnderCursor();

	// Вариант A (решение Рината):
	//  - тап по валидному врагу -> РУЧНОЙ фокус именно на нём (держится до смерти/смены);
	//  - тап по другому врагу    -> сменить ручной фокус на него;
	//  - тап по ПУСТОМУ месту     -> снять ручной фокус, вернуться в авто-ближайшую.
	if (IsValidTarget(HitActor))
	{
		CurrentTarget = HitActor;
		bManualLock = true;
		UE_LOG(LogTemp, Warning, TEXT("Manual target locked: %s"), *CurrentTarget->GetName());
	}
	else
	{
		// Пустое место: снимаем ручной фокус, авто подберёт ближайшую (в UpdateAutoTarget/ниже).
		bManualLock = false;
		CurrentTarget = FindNearestLivingTarget();
	}
}

bool AContrarySurvivorPlayerController::IsValidTarget(AActor* Target) const
{
	// ТИП-АГНОСТИЧНО: цель валидна, если несёт UStatsComponent (любой враг — бандит,
	// волк, …), это не сам игрок и он жив.
	const UStatsComponent* TargetStats = GetTargetStats(Target);
	return TargetStats && !TargetStats->IsDead();
}

UStatsComponent* AContrarySurvivorPlayerController::GetTargetStats(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	// Сам игрок целью быть не может (у игрока тоже есть UStatsComponent).
	if (Actor == GetPawn())
	{
		return nullptr;
	}

	// «Врага» определяем по наличию компонента, а НЕ по конкретному классу —
	// подходит и AEnemyCharacter (бандит), и AWolfCharacter (волк), и будущим врагам.
	return Actor->FindComponentByClass<UStatsComponent>();
}

AActor* AContrarySurvivorPlayerController::FindNearestLivingTarget() const
{
	UWorld* World = GetWorld();
	APawn* PlayerPawn = GetPawn();
	if (!World || !PlayerPawn)
	{
		return nullptr;
	}

	const FVector Origin = PlayerPawn->GetActorLocation();
	const float RadiusSq = AutoTargetRadius * AutoTargetRadius;

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();

	// Все Pawn'ы со StatsComponent (живые, не игрок) — берём ближайшего в радиусе.
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Candidate = *It;
		if (!IsValid(Candidate) || Candidate == PlayerPawn)
		{
			continue;
		}

		const UStatsComponent* CandidateStats = Candidate->FindComponentByClass<UStatsComponent>();
		if (!CandidateStats || CandidateStats->IsDead())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Origin, Candidate->GetActorLocation());
		if (DistSq <= RadiusSq && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	}

	return Best;
}

AActor* AContrarySurvivorPlayerController::GetActorUnderCursor()
{
	FHitResult HitResult;

	// LineTrace под позицией курсора мыши
	bool bHit = GetHitResultUnderCursor(ECC_Pawn, false, HitResult);

	if (bHit && HitResult.GetActor())
	{
		return HitResult.GetActor();
	}

	return nullptr;
}

void AContrarySurvivorPlayerController::Interact(const FInputActionValue& Value)
{
	// TODO: реализовать взаимодействие с предметами и NPC
}

void AContrarySurvivorPlayerController::Inventory(const FInputActionValue& Value)
{
	// TODO: открыть/закрыть инвентарь
}

#undef LOCTEXT_NAMESPACE
