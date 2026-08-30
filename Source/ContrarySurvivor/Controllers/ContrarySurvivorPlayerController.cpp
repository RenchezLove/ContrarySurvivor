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
#include "Components/AudioComponent.h" // заглушение звуков мира на время меню (08-09)
#include "Sound/SoundBase.h"                 // трек музыки паузы (мягкая ссылка)
#include "Sound/SoundWave.h"                 // задача №5: форс bLooping у боевого трека (приём StartAmbience)
#include "UObject/UObjectIterator.h"    // обход живых звуков мира
#include "ContrarySurvivor/Actors/ShopTypes.h"          // FShopEntry (каталог в OnQABuyCheapest, A2)
#include "ContrarySurvivor/Actors/ShopVendor.h"         // IShopVendor / UShopVendor (вендор магазина, A2)
#include "ContrarySurvivor/Actors/ElderNPC.h"           // Фаза 5: староста (диалог/квест)
#include "ContrarySurvivor/Components/QuestComponent.h"  // Фаза 5: журнал квестов игрока
#include "ContrarySurvivor/Components/CorpseLootComponent.h" // Build 1.2.1 (А1): обыск трупов
#include "ContrarySurvivor/UI/InventoryScreenWidget.h" // ADR-082: SetPanelMode при связке окон
#include "ContrarySurvivor/UI/CorpseLootWidget.h"      // ADR-082: окно-напарник режима Search
#include "ContrarySurvivor/UI/ShopScreenWidget.h"      // ADR-082: окно-напарник режима Trade
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/Actors/VillageZone.h"          // ADR-074: граница деревни для подсказки о сохранении
#include "ContrarySurvivor/Characters/WolfCharacter.h"   // QA: спавн тест-волка (клавиша B)
#include "ContrarySurvivor/Subsystems/SpawnPlacementUtils.h" // QA: трасса до пола (телепорт V)
#include "ContrarySurvivor/Controllers/EnemyAIController.h" // QA headless-тест погони: режим/дальность
#include "NavigationSystem.h"  // QA headless-тест: проекция враг/игрок на навмеш (selfNav/targetNav)
#include "TimerManager.h"      // QA headless-тест: таймер-сэмплинг
#include "ContrarySurvivor/Debug/QADebug.h"              // QA debug-флаги/хелпер (T/Z/B/O/N)
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Engine/Engine.h"                      // GEngine->Exec (подавление экранного спама)
#include "BrainComponent.h"    // QA freeze (U): PauseLogic/ResumeLogic (на случай BT-врагов; у наших мозг — state-machine)
#include "HAL/IConsoleManager.h" // QA Preview-тумблер (Period): cvar ShowFlag.PreviewShadowsIndicator
#include "Misc/Paths.h"        // QA screenshot (G): запасная папка снимков
#include "ContrarySurvivor/Debug/ContraryDebugCamera.h" // F1: наш cheat-manager с тихой свободной камерой
#if CONTRARY_WITH_QA_CHEATS && PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <ShlObj.h>            // QA screenshot (G): SHGetKnownFolderPath(FOLDERID_Desktop)
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#include "ContrarySurvivor/Retention/OnboardingComponent.h" // Этап F1: онбординг-подсказки
#include "ContrarySurvivor/Retention/DailyRewardComponent.h" // Build 1: отложенное окно ежедневки (после интро-диалога)
#include "ContrarySurvivor/UI/TouchControlsWidget.h"        // Этап G: виртуальный стик (Android)
#include "ContrarySurvivor/UI/PauseMenuWidget.h"            // Этап G: меню паузы
#include "ContrarySurvivor/UI/StartScreenWidget.h"          // Главное меню (ADR-062; вырос из стартового экрана Б3)
#include "ContrarySurvivor/UI/SettingsScreenWidget.h"       // Экран настроек (ADR-062, подход 2)
#include "ContrarySurvivor/UI/SupportAuthorWidget.h"        // Окно «Поддержать автора» (задание издателя)
#include "ContrarySurvivor/Ads/AdService.h"                 // показ ролика в окне «Поддержать автора»
#include "ContrarySurvivor/Settings/ContrarySurvivorGameUserSettings.h" // применение настроек при запуске
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h"  // маркер «игра уже запускалась» (меню со второго запуска)
#include "ContrarySurvivor/Analytics/DataConsentSubsystem.h" // ждём ли ответа про сбор данных (загрузочный уровень)
#include "ContrarySurvivor/Subsystems/GameFlowSubsystem.h"   // намерение перехода в мир (ADR-067 п.5)
#include "ContrarySurvivor/UI/IntroScreenWidget.h"          // Build 1: экран интро (чёрный + строки)
#include "Blueprint/UserWidget.h"                            // CreateWidget
#include "Kismet/KismetSystemLibrary.h"                      // QuitGame («Выход» меню паузы)
#include "Misc/PackageName.h"                                // короткое имя уровня из полного адреса
#include "Camera/PlayerCameraManager.h"                       // чёрный кадр на загрузочном уровне

// Пространство имён переводов для литералов этого файла (ADR-050): подписи тач-кнопок.
#define LOCTEXT_NAMESPACE "ContrarySurvivorPlayerController"

AContrarySurvivorPlayerController::AContrarySurvivorPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentTarget = nullptr;

	// Свободная камера F1 — наш тихий класс (без надписей/линий/смены вьюмода): движок берёт
	// класс камеры из UCheatManager::DebugCameraControllerClass, поэтому подменяем cheat-manager.
	// В Shipping cheat-manager движком не создаётся вовсе (UE_WITH_CHEAT_MANAGER), клавиши F1 нет.
	CheatClass = UContraryCheatManager::StaticClass();

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

	// На каком уровне мы оказались — узнаём ОДИН раз и запоминаем (ADR-067 п.5). Имя карты
	// берём без служебной приставки редактора, иначе запуск из редактора не узнал бы сам себя.
	bOnBootLevel = IsSameLevel(
		UGameplayStatics::GetCurrentLevelName(this, /*bRemovePrefixString=*/true), BootLevelPath);
	if (bOnBootLevel)
	{
		UE_LOG(LogQA, Display,
			TEXT("QA: запуск на загрузочном уровне '%s' — мир не грузим, решение примем на месте"),
			*BootLevelPath.ToString());

		// Кадр делаем ЧЁРНЫМ (решение game-lead 08-12): пустая сцена с небом читается как
		// поломка, чёрный кадр — как обычная загрузка. Затемнение камеры кладётся поверх
		// трёхмерной сцены, но ПОД окнами интерфейса (LocalPlayer.cpp:753 движка 5.5 — цвет
		// уходит в OverlayColor вида), поэтому главное меню и экран согласия поверх него видны.
		if (PlayerCameraManager)
		{
			PlayerCameraManager->SetManualCameraFade(1.0f, FLinearColor::Black, /*bInFadeAudio=*/false);
		}

		// ADR-076 п.4: музыка меню — с ПЕРВОГО кадра загрузочного уровня (ещё под
		// заставкой-логотипом), а не с появления меню. Гейт знает про bOnBootLevel.
		ApplyMenuMusicGate();
	}

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
	// На загрузочном уровне экранные кнопки не нужны: управлять нечем, персонажа нет.
	if (bWantTouchLayer && !bOnBootLevel)
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

	// Опрос боевой музыки (задача №5, ТЗ 30.08). Тикер ЯДРА, а не таймер мира: таймеры мира
	// стоят на паузе, а гейт тишины обязан работать и под ней (реклама-заглушка ставит
	// SetGamePaused без уведомления). Приём — как у ContraryShowcaseCheats/DataConsentWait.
	{
		TWeakObjectPtr<AContrarySurvivorPlayerController> WeakThis(this);
		CombatMusicTickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("ContraryCombatMusic"),
			FMath::Max(0.1f, CombatMusicPollPeriod),
			[WeakThis](float DeltaSeconds) -> bool
			{
				AContrarySurvivorPlayerController* Self = WeakThis.Get();
				return Self ? Self->TickCombatMusic(DeltaSeconds) : false;
			});
	}
}

void AContrarySurvivorPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Тикер живёт в ядре движка и со смертью контроллера сам не исчезает — снимаем руками.
	if (CombatMusicTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(CombatMusicTickerHandle);
		CombatMusicTickerHandle.Reset();
	}
	// Музыку глушим тут же: компонент и так умрёт вместе с миром (ошибка 6 ТЗ), но при
	// переезде уровней аккуратная остановка дешевле догадок, кто кого переживёт.
	StopCombatMusicImmediately();
	Super::EndPlay(EndPlayReason);
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
		// 2026-08-15 (Ринат): оставлен только набор для съёмки. Обработчики F2/F3/F4/F6/F7/F9/F10/F12,
		// C/X/Z/M/V/J/H/B/N/P/O и запятой удалены вместе с привязками (были: тест-предметы, броня,
		// телепорты, магазин, квесты, force-drop, спавн волка, оверлей, force-kill, убить игрока,
		// стереть сейв). Legacy ActionMapping — Config/DefaultInput.ini.
		// F1 — свободная камера (класс — наш AContraryDebugCameraController через UContraryCheatManager).
		InputComponent->BindAction(TEXT("QAToggleDebugCam"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnToggleDebugCamera);
		// K — +TestMoneyGrant денег (K, а не F5: F5 в PIE — вьюмод Shader Complexity).
		InputComponent->BindAction(TEXT("QAGiveMoney"),      IE_Pressed, this, &AContrarySurvivorPlayerController::OnTestGiveMoney);
		// T — god-mode (неуязвимость + заморозка голода/жажды).
		InputComponent->BindAction(TEXT("QAGodMode"),      IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAToggleGodMode);

		// Дебаг-клавиши Рината (2026-08-14): G — снимок игровой области на рабочий стол; Y — статы 100%;
		// U — тумблер заморозки всех врагов.
		// bExecuteWhenPaused (2026-08-15, Ринат): главное меню, меню паузы и настройки держат мир на
		// паузе, а привязки без этого флага при паузе не вызываются (PlayerInput.cpp:940) — G в меню
		// молчал. Снимок нужен и там (кадры магазина), поэтому флаг ставим только на G.
		FInputActionBinding& ScreenshotBinding = InputComponent->BindAction(TEXT("QAScreenshot"), IE_Pressed,
			this, &AContrarySurvivorPlayerController::OnQAScreenshot);
		ScreenshotBinding.bExecuteWhenPaused = true;
		InputComponent->BindAction(TEXT("QAFullStats"),     IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAFullStats);
		InputComponent->BindAction(TEXT("QAFreezeEnemies"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAToggleFreezeEnemies);

		// Period (2026-08-15): тумблер надписей «Preview» в тенях непостроенного света (для съёмки).
		InputComponent->BindAction(TEXT("QATogglePreviewShadows"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnQATogglePreviewShadows);
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

	// Боевая музыка гаснет СРАЗУ (ошибка 3 ТЗ 30.08: не играет на экране смерти). Здесь —
	// мгновенная тишина паузой; совсем остановит и отпустит компонент ближайший тик опроса.
	if (CombatMusicComponent && !bCombatMusicGatePaused)
	{
		CombatMusicComponent->SetPaused(true);
		bCombatMusicGatePaused = true;
	}

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
	// ⛔ ПОРЯДОК ПРОВЕРОК ВАЖЕН. Окно «Поддержать автора» лежит выше всех прочих, поэтому оно
	// проверяется ПЕРВЫМ — раньше даже загрузочного уровня: окно открывается и оттуда, из
	// главного меню, и закрыть его игрок обязан суметь.
	//
	// Окно закрывается кнопкой «Назад» на телефоне и клавишей Esc на компьютере — это
	// требование задания издателя. Отдельной обработки кнопки «Назад» заводить не пришлось:
	// в проекте она уже привязана к этому же действию (Config/DefaultInput.ini, строка
	// Key=Android_Back у действия PauseMenu), и точно так же здесь закрывается окно обыска трупа.
	if (bSupportScreenOpen)
	{
		CloseSupportScreen();
		return;
	}
	// На загрузочном уровне ставить на паузу нечего: мира нет. Кнопка «назад» на телефоне и
	// клавиша Esc здесь просто молчат, иначе поверх главного меню открылась бы пауза без игры.
	if (bOnBootLevel)
	{
		return;
	}
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
		PauseMenuWidget->OnMainMenuRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandlePauseMainMenu);
		// Просьба Рината 08-09: настройки открываются и из паузы — тем же методом, что из
		// главного меню. Пункт в панели появляется САМ по факту этой привязки.
		PauseMenuWidget->OnSettingsRequested.AddUObject(this, &AContrarySurvivorPlayerController::OpenSettingsScreen);
		// Задание издателя: вторая точка входа в окно «Поддержать автора» — из паузы.
		PauseMenuWidget->OnSupportRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandlePauseSupportRequested);
	}

	// Есть ли что терять — решает игрок-персонаж (сравнивает заработанное с последним
	// сохранением). Освежаем при КАЖДОМ открытии паузы: между открытиями игрок мог и
	// сохраниться у костра, и снова набрать добра.
	{
		const APlayerCharacter* PausePawn = Cast<APlayerCharacter>(GetPawn());
		PauseMenuWidget->SetProgressUnsaved(!PausePawn || PausePawn->IsProgressUnsaved());
	}
	// Z=60: поверх окна ежедневки (50) и остального UI.
	PauseMenuWidget->AddToViewport(/*ZOrder=*/60);

	bPauseMenuOpen = true;
	bUIClickConsumed = false;

	// Музыка паузы: звучит по кругу, пока панель на экране (просьба Рината 08-09).
	ApplyMenuMusicGate();

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

void AContrarySurvivorPlayerController::ApplyMenuMusicGate()
{
	// Трек играет, пока на экране ЛЮБОЕ меню: пауза, главное меню или настройки поверх них
	// (просьба Рината 08-09: «музыка должна играть и в главном меню»). Трек один и тот же,
	// поле ссылки и поле громкости тоже одни — вторых не заводим.
	// ADR-076 п.4 (Ринат: «музыка начинала играть прямо в тот момент, когда показывается мой
	// логотип»): на ЗАГРУЗОЧНОМ уровне трек играет ВСЕГДА с первого кадра — на телефоне это
	// хвост показа заставки-логотипа (Build/Android/res/drawable/splashscreen_landscape.png
	// гаснет на первом отрисованном кадре, а звук стартует в BeginPlay ещё под ней) — к
	// появлению меню долгое вступление трека уже отыграло. Раньше движок звучать физически
	// не может: пока заставка на экране, он ещё загружается. Заодно трек честно играет и на
	// экране согласия, который на первом запуске стоит раньше меню.
	const bool bWantMusic = bPauseMenuOpen || IsMainMenuOnScreen() || bHoldMenuMusicThroughTransition
		|| bOnBootLevel;
	if (bWantMusic)
	{
		StartPauseMusic(); // при уже играющем треке метод сам ничего не делает
	}
	else
	{
		StopPauseMusic();
	}
}

void AContrarySurvivorPlayerController::StartPauseMusic()
{
	if (PauseMusic.IsNull())
	{
		return; // поле пустое — музыки в паузе нет, это допустимая настройка
	}
	if (PauseMusicComponent && PauseMusicComponent->IsPlaying())
	{
		return; // уже играет (пауза могла открыться поверх другой модалки)
	}

	// Подтягиваем звук в момент открытия паузы: ссылка мягкая, потому что у трека включена
	// подгрузка по ходу воспроизведения и держать его в памяти телефона всю игру незачем.
	USoundBase* Music = PauseMusic.LoadSynchronous();
	if (!Music)
	{
		// Так и вышло на телефоне 08-09: трека в собранном пакете не оказалось вовсе.
		// Причина типовая, поэтому пишем сразу и её: на МЯГКУЮ ссылку упаковщик не смотрит,
		// и папку со звуком нужно перечислить в списке обязательной упаковки
		// (Config/DefaultGame.ini, «Additional Asset Directories to Cook»).
		UE_LOG(LogQA, Warning,
			TEXT("QA: трек паузы не загрузился ('%s') — пауза будет без музыки. Если в редакторе он есть, а в собранной игре нет — проверь, что его папка перечислена в списке обязательной упаковки. [pause-music: load failed]"),
			*PauseMusic.ToString());
		return;
	}

	// SpawnSound2D — ровно то, что нужно на паузе: звук плоский (без привязки к точке мира)
	// и помечается движком как ЗВУК ИНТЕРФЕЙСА (GameplayStatics.cpp:1669). Именно поэтому он
	// продолжает звучать при остановленном мире: на паузе движок глушит только звуки самого
	// мира (FAudioDevice::HandlePause, AudioDevice.cpp:4438 — звуки интерфейса не трогает).
	// Зацикливание берётся из самого ассета звука.
	PauseMusicComponent = UGameplayStatics::SpawnSound2D(this, Music,
		FMath::Max(0.0f, PauseMusicVolume), /*PitchMultiplier=*/1.0f, /*StartTime=*/0.0f,
		/*ConcurrencySettings=*/nullptr, /*bPersistAcrossLevelTransition=*/false,
		/*bAutoDestroy=*/false);

	UE_LOG(LogQA, Display, TEXT("QA: музыка паузы включена ('%s', громкость %.2f)"),
		*Music->GetName(), PauseMusicVolume);
}

void AContrarySurvivorPlayerController::StopPauseMusic()
{
	if (!PauseMusicComponent)
	{
		return;
	}
	PauseMusicComponent->Stop();
	PauseMusicComponent = nullptr; // создан с bAutoDestroy=false — освобождаем ссылку сами
	UE_LOG(LogQA, Display, TEXT("QA: музыка паузы выключена"));
}

// ---------------------------------------------------------------------------
// Боевая музыка (задача №5, ТЗ издателя 30.08)
// ---------------------------------------------------------------------------

int32 AContrarySurvivorPlayerController::PickCombatTrackIndex(int32 LastTrackIndex, bool bTrackAValid, bool bTrackBValid)
{
	if (!bTrackAValid && !bTrackBValid)
	{
		return INDEX_NONE; // оба поля пустые — боевой музыки нет (допустимая настройка)
	}
	if (!bTrackAValid)
	{
		return 1;
	}
	if (!bTrackBValid)
	{
		return 0;
	}
	// Оба трека на месте: берём тот, которого не было в прошлом бою; самый первый бой
	// (LastTrackIndex = INDEX_NONE) начинает с первого трека.
	return LastTrackIndex == 0 ? 1 : 0;
}

bool AContrarySurvivorPlayerController::ShouldCountEnemyForCombatMusic(bool bEngaging, float DistSquared, float RadiusSquared)
{
	// «Рядом с игроком есть живой враг в бою»: боевое состояние + дистанция в радиусе.
	// Живость отдельно не передаётся — мёртвый враг выпадает из реестра сам (UnPossess).
	return bEngaging && DistSquared <= RadiusSquared;
}

bool AContrarySurvivorPlayerController::ShouldGateSilenceCombatMusic(bool bWorldPaused, bool bMainMenuOnScreen, bool bDeathScreen)
{
	// Пауза мира ловит и меню паузы, и рекламу-заглушку (та ставит SetGamePaused); реальная
	// рекламная сеть глушит весь звук игры сама (FApp::SetVolumeMultiplier(0)). Обыск и
	// торговец мир НЕ останавливают — там музыка продолжает играть (ошибка 4 ТЗ).
	return bWorldPaused || bMainMenuOnScreen || bDeathScreen;
}

bool AContrarySurvivorPlayerController::IsAnyEnemyEngagingNearby() const
{
	const APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		return false;
	}

	const FVector MyLocation = MyPawn->GetActorLocation();
	const float RadiusSquared = FMath::Square(CombatMusicNearbyRadius);

	// Реестр живых контроллеров врагов — единицы записей; сравнение состояния и квадрата
	// дистанции. Никакого обхода всех акторов мира — это и есть «оптимизированно» из ТЗ
	// (ошибка 2: проверка-страховка не должна подлагивать игру).
	for (const TWeakObjectPtr<AEnemyAIController>& WeakEnemy : AEnemyAIController::GetActiveControllers())
	{
		const AEnemyAIController* Enemy = WeakEnemy.Get();
		const APawn* EnemyPawn = Enemy ? Enemy->GetPawn() : nullptr;
		if (!EnemyPawn)
		{
			continue;
		}
		if (ShouldCountEnemyForCombatMusic(Enemy->IsEngagingPlayer(),
			FVector::DistSquared(MyLocation, EnemyPawn->GetActorLocation()), RadiusSquared))
		{
			return true;
		}
	}
	return false;
}

void AContrarySurvivorPlayerController::OnEnemyEnteredCombat(AEnemyAIController* Enemy)
{
	// Вход в бой — мгновенный старт музыки, без ожидания тика опроса («между условием и первой
	// нотой не должно быть заметной паузы»). Под гейтом тишины новый бой музыку не заводит:
	// на экране смерти она не нужна, а на паузе ИИ и не тикает.
	UWorld* World = GetWorld();
	if (!World || bDeathScreen || IsMainMenuOnScreen() || World->IsPaused())
	{
		return;
	}

	const APawn* MyPawn = GetPawn();
	const APawn* EnemyPawn = Enemy ? Enemy->GetPawn() : nullptr;
	if (!MyPawn || !EnemyPawn)
	{
		return;
	}

	// Радиусный фильтр «рядом»: далёкий бой (Standoff где-то за экраном) музыку не включает.
	if (!ShouldCountEnemyForCombatMusic(true,
		FVector::DistSquared(MyPawn->GetActorLocation(), EnemyPawn->GetActorLocation()),
		FMath::Square(CombatMusicNearbyRadius)))
	{
		return;
	}

	StartOrRecoverCombatMusic();
}

void AContrarySurvivorPlayerController::StartOrRecoverCombatMusic()
{
	if (CombatMusicComponent && CombatMusicComponent->IsPlaying())
	{
		if (bCombatMusicFadingOut)
		{
			// Ошибка 1 ТЗ: бой возобновился во время затихания — музыка возвращается немедленно
			// и на полную громкость, БЕЗ рестарта трека. AdjustVolume с ненулевой целью снимает
			// запрос остановки (ActiveSound.FadeOut = None) и поднимает громкость затухателя
			// обратно к единице (сверено с движком 5.5: AudioComponent.cpp,
			// AdjustVolumeInternal + ActiveSound.cpp, проверка EFadeOut::None). Короткие 0.2 с —
			// «немедленно» на слух, но без щелчка.
			CombatMusicComponent->AdjustVolume(0.2f, 1.0f);
			bCombatMusicFadingOut = false;
			UE_LOG(LogQA, Display, TEXT("QA: боевая музыка возвращена из затихания (бой возобновился)"));
		}
		// Уже играет — ничего: два врага, заметившие игрока одновременно, дают ОДИН трек,
		// а не две наложенные копии (ошибка 7 ТЗ).
		return;
	}

	StartCombatMusicForNewFight();
}

void AContrarySurvivorPlayerController::StartCombatMusicForNewFight()
{
	const int32 Pick = PickCombatTrackIndex(LastCombatTrackIndex,
		!CombatMusicTrackA.IsNull(), !CombatMusicTrackB.IsNull());
	if (Pick == INDEX_NONE)
	{
		return; // оба поля пустые — боевой музыки нет
	}

	const TSoftObjectPtr<USoundBase>& TrackRef = (Pick == 0) ? CombatMusicTrackA : CombatMusicTrackB;
	USoundBase* Music = TrackRef.LoadSynchronous();
	if (!Music)
	{
		// Типовая причина на телефоне: на мягкую ссылку упаковщик не смотрит — папка звука
		// должна быть в списке обязательной упаковки (Config/DefaultGame.ini). Папка
		// /Game/Audio/Music там уже есть, но страховка от переименования ассета не помешает.
		UE_LOG(LogQA, Warning,
			TEXT("QA: боевой трек не загрузился ('%s') — бой пойдёт без музыки [combat-music: load failed]"),
			*TrackRef.ToString());
		return;
	}

	// Зацикливание в UE 5.5 — свойство САМОГО ассета (USoundWave::bLooping); у SpawnSound2D
	// параметра loop нет. Форсим на загруженном ассете (приём StartAmbience): трек доиграл —
	// начинается снова без паузы, циклы у волн сшиты при импорте.
	if (USoundWave* Wave = Cast<USoundWave>(Music))
	{
		Wave->bLooping = true;
	}

	// Сразу на полной громкости — «никакого нарастания» (ТЗ). Звук плоский, без точки мира.
	// bPersistAcrossLevelTransition=false — перезапуск уровня музыку не переживёт (ошибка 6);
	// bAutoDestroy=false — компонент держим ссылкой и отпускаем сами (опрос/смерть/EndPlay).
	CombatMusicComponent = UGameplayStatics::SpawnSound2D(this, Music,
		FMath::Max(0.0f, CombatMusicVolume) * UContrarySurvivorGameUserSettings::GetMusicVolumeSafe(),
		/*PitchMultiplier=*/1.0f, /*StartTime=*/0.0f, /*ConcurrencySettings=*/nullptr,
		/*bPersistAcrossLevelTransition=*/false, /*bAutoDestroy=*/false);
	if (!CombatMusicComponent)
	{
		return;
	}

	bCombatMusicFadingOut = false;
	bCombatMusicGatePaused = false;
	LastCombatTrackIndex = Pick;

	UE_LOG(LogQA, Display, TEXT("QA: боевая музыка включена ('%s', трек %d, громкость %.2f)"),
		*Music->GetName(), Pick + 1, CombatMusicVolume);
}

void AContrarySurvivorPlayerController::StopCombatMusicImmediately()
{
	bCombatMusicFadingOut = false;
	bCombatMusicGatePaused = false;
	if (!CombatMusicComponent)
	{
		return;
	}
	CombatMusicComponent->Stop();
	CombatMusicComponent = nullptr; // создан с bAutoDestroy=false — освобождаем ссылку сами
	UE_LOG(LogQA, Display, TEXT("QA: боевая музыка остановлена"));
}

bool AContrarySurvivorPlayerController::TickCombatMusic(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return true; // мир в переезде — просто ждём следующего тика (тикер снимет EndPlay)
	}

	// Уборка: затихание дошло до нуля и звук остановился сам (FadeOut на нуле останавливает
	// активный звук) — отпускаем компонент. Приостановленный гейтом компонент сюда не попадёт:
	// на паузе IsPlaying() у него по-прежнему true (IsActive).
	if (CombatMusicComponent && !CombatMusicComponent->IsPlaying())
	{
		CombatMusicComponent = nullptr;
		bCombatMusicFadingOut = false;
		bCombatMusicGatePaused = false;
		UE_LOG(LogQA, Display, TEXT("QA: боевая музыка дозатихла и выключена (бой кончился)"));
	}

	// Смерть игрока: мгновенную тишину дал ShowDeathScreen (SetPaused), здесь останавливаем
	// совсем — после возрождения бой начнётся заново, с новым треком, а не с хвоста затихания.
	if (bDeathScreen)
	{
		StopCombatMusicImmediately();
	}

	const bool bCombatNearby = !bDeathScreen && IsAnyEnemyEngagingNearby();

	// Гейт тишины (идемпотентно): пауза мира (меню паузы, реклама-заглушка) или главное меню —
	// музыка приостанавливается, НЕ останавливается: гейт снят — бой продолжается со звуком.
	// Руками, потому что звук из SpawnSound2D помечен звуком интерфейса и пауза мира его не глушит.
	const bool bGateSilence = ShouldGateSilenceCombatMusic(
		World->IsPaused(), IsMainMenuOnScreen(), bDeathScreen);
	if (CombatMusicComponent && bGateSilence != bCombatMusicGatePaused)
	{
		CombatMusicComponent->SetPaused(bGateSilence);
		bCombatMusicGatePaused = bGateSilence;
	}

	if (!bGateSilence)
	{
		// Рядом боя нет — плавное затихание (2.5 с по умолчанию), потом звук остановится сам.
		// Тот же опрос — страховка от «залипла навечно» (ошибка 2 ТЗ): даже если событие
		// окончания боя не пришло, отсутствие врагов рядом выключит музыку.
		if (CombatMusicComponent && !bCombatMusicFadingOut && !bCombatNearby)
		{
			CombatMusicComponent->FadeOut(FMath::Max(0.1f, CombatMusicFadeOutSeconds), 0.0f);
			bCombatMusicFadingOut = true;
			UE_LOG(LogQA, Display, TEXT("QA: боевая музыка затихает (%.1f с) — рядом не осталось врагов в бою"),
				CombatMusicFadeOutSeconds);
		}

		// Бой рядом, а музыка молчит или гаснет — тот же путь, что событие входа в бой.
		// Закрывает дыру событийного старта: игрок вернулся к врагу, который бой и не
		// прекращал, — события «вход в бой» не будет, музыку заводит опрос.
		if (bCombatNearby)
		{
			StartOrRecoverCombatMusic();
		}

		// Ползунок громкости музыки на лету (пока не идёт затихание — его не перетираем).
		if (CombatMusicComponent && !bCombatMusicFadingOut)
		{
			CombatMusicComponent->SetVolumeMultiplier(
				FMath::Max(0.0f, CombatMusicVolume) * UContrarySurvivorGameUserSettings::GetMusicVolumeSafe());
		}
	}

	// Приглушение лесного фона на время боя (ТЗ: «на 30 %, после боя возвращается»).
	// Плавно: полный ход множителя [DuckFactor..1] проходит за CombatAmbienceDuckLerpSeconds.
	const float DuckTarget = bCombatNearby ? FMath::Clamp(CombatAmbienceDuckFactor, 0.0f, 1.0f) : 1.0f;
	CombatAmbienceDuckCurrent = FMath::FInterpConstantTo(CombatAmbienceDuckCurrent, DuckTarget,
		DeltaTime, 1.0f / FMath::Max(0.05f, CombatAmbienceDuckLerpSeconds));
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		PlayerChar->SetAmbienceCombatDuck(CombatAmbienceDuckCurrent);
	}

	return true; // тикер живёт до EndPlay
}

void AContrarySurvivorPlayerController::ClosePauseMenu()
{
	if (!bPauseMenuOpen)
	{
		return;
	}
	bPauseMenuOpen = false;
	bUIClickConsumed = false;

	ApplyMenuMusicGate();

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

void AContrarySurvivorPlayerController::HandlePauseMainMenu()
{
	// Сюда попадаем, когда решение окончательное: при несохранённом прогрессе виджет паузы
	// уже переспросил сам (спека, раздел «Поведение паузы»).
	UE_LOG(LogQA, Display, TEXT("QA: pause menu -> MAIN MENU"));

	// Переезд запуска (ADR-067 п.5): «Главное меню» из паузы теперь ВОЗВРАЩАЕТ на загрузочный
	// уровень, а не открывает меню поверх живого мира. Иначе весь мир остался бы в памяти
	// телефона — а это ровно половина смысла всей работы.
	// Музыку меню на этом переходе не удерживаем намеренно: мир выгружается вместе со звуком,
	// и трек всё равно оборвётся — заведётся он заново уже на загрузочном уровне.
	if (!BootLevelPath.IsNone() && !bOnBootLevel)
	{
		ClosePauseMenu();
		TravelToBootLevel();
		return;
	}

	// Запасной путь (адрес загрузочного уровня стёрли в настройках либо мы уже на нём) —
	// прежнее поведение: меню открывается поверх текущего уровня.
	// Музыка НЕ обрывается на этом переходе: между закрытием паузы и открытием меню на один
	// шаг не открыто ни то, ни другое, и гейт остановил бы трек, а меню завело бы его заново
	// с начала. Держим его на время перехода (просьба Рината: пусть продолжает играть).
	bHoldMenuMusicThroughTransition = true;
	ClosePauseMenu();
	// Главное меню само поставит паузу заново и обновит наличие сохранения (оттуда игрок
	// выберет «Продолжить» — загрузку последнего сохранения — или «Новую игру»).
	OpenStartScreen();
	bHoldMenuMusicThroughTransition = false;
	ApplyMenuMusicGate();
}

void AContrarySurvivorPlayerController::HandlePauseQuit()
{
	UE_LOG(LogQA, Display, TEXT("QA: pause menu QUIT pressed — quitting game"));
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}

// ---------------------------------------------------------------------------
// Загрузочный уровень (переезд запуска, ADR-067 п.5).
//
// Смысл: раньше игра стартовала прямо на игровой карте, поэтому телефон грузил лес и деревню
// ДО главного меню — были слышны птицы, тратились время и батарея. Теперь запуск идёт на
// пустом уровне L_Boot, а мир грузится только после «Продолжить» или «Новая игра».
//
// ⛔ Решение «первый это запуск после установки или нет» принимается ЗДЕСЬ и ровно один раз:
// признак «игра уже запускалась» не читается, а ставится в момент чтения. Дальше в мир едет
// явное намерение через UGameFlowSubsystem, и мировой контроллер заново ничего не решает.
// ---------------------------------------------------------------------------

bool AContrarySurvivorPlayerController::IsSameLevel(const FString& CurrentShortName, FName LevelPath)
{
	if (LevelPath.IsNone() || CurrentShortName.IsEmpty())
	{
		return false;
	}
	// Короткое имя из адреса: «/Game/Maps/L_Boot» -> «L_Boot», «L_Boot» -> «L_Boot».
	const FString Short = FPackageName::GetShortName(LevelPath.ToString());
	return CurrentShortName.Equals(Short, ESearchCase::IgnoreCase);
}

EContraryWorldEntryIntent AContrarySurvivorPlayerController::BootIntentForLaunch(
	bool bLaunchedBefore, bool bHasSave)
{
	// Правило то же, что и до переезда (ADR-062): главное меню показывается со второго запуска
	// после установки либо когда найдено сохранение. Изменился только смысл ответа «меню не
	// нужно»: теперь это «сразу везём игрока в мир новой игрой», а не «играем вступление тут же».
	return ShouldShowMainMenuOnLaunch(bLaunchedBefore, bHasSave)
		? EContraryWorldEntryIntent::None
		: EContraryWorldEntryIntent::NewGame;
}

void AContrarySurvivorPlayerController::UpdateBootFlow()
{
	if (bBootDecisionMade)
	{
		return; // решение уже принято, дальше распоряжается либо меню, либо переезд в мир
	}

	// Согласие на сбор данных спрашивается раньше всего остального. Пока игрок не ответил,
	// главное меню не открываем: два окна друг поверх друга ему показывать нельзя. Ожидание
	// конечно при любом раскладе — подсистема сама сдаётся, если экрана так и не появилось.
	if (const UDataConsentSubsystem* Consent = UDataConsentSubsystem::Get(this))
	{
		if (Consent->IsWaitingForPlayerAnswer())
		{
			return;
		}
	}

	bBootDecisionMade = true; // ровно один раз за запуск

	bool bLaunchedBefore = false;
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		// ⚠ Этот вызов не спрашивает, а ПОМЕЧАЕТ запуск. Второй раз за игру звать его нельзя.
		bLaunchedBefore = Analytics->MarkLaunchAndCheckWasLaunchedBefore();
	}
	const bool bHasSave = APlayerCharacter::HasDefaultSaveGame();

	const EContraryWorldEntryIntent Intent = BootIntentForLaunch(bLaunchedBefore, bHasSave);
	UE_LOG(LogQA, Display,
		TEXT("QA: загрузочный уровень — игра запускалась раньше: %s, сохранение есть: %s -> %s"),
		bLaunchedBefore ? TEXT("да") : TEXT("нет"),
		bHasSave ? TEXT("да") : TEXT("нет"),
		Intent == EContraryWorldEntryIntent::None ? TEXT("главное меню") : TEXT("сразу в мир"));

	if (Intent == EContraryWorldEntryIntent::None)
	{
		OpenStartScreen();
		return;
	}

	// Самый первый запуск после установки: меню игрок не видит вовсе, едем прямо в мир.
	TravelToGameWorld(EContraryWorldEntryIntent::NewGame);
}

void AContrarySurvivorPlayerController::TravelToGameWorld(EContraryWorldEntryIntent Intent)
{
	if (UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this))
	{
		Flow->SetWorldEntryIntent(Intent);
	}
	else
	{
		// Намерение передать некому — мир примет решение сам, по прежнему правилу. Хуже, но не
		// смертельно: игрок в любом случае попадёт в игру.
		UE_LOG(LogQA, Warning,
			TEXT("QA: намерение перехода передать некому — мир решит по-старому"));
	}

	// Паузу снимаем ПЕРЕД переездом: на загрузочном уровне её мог поставить экран согласия или
	// само меню, а ехать в мир с остановленным временем незачем.
	if (IsPaused())
	{
		SetPause(false);
	}

	UE_LOG(LogQA, Display, TEXT("QA: едем в игровой мир '%s' (%s)"),
		*GameWorldLevelPath.ToString(), *UGameFlowSubsystem::IntentToString(Intent));
	UGameplayStatics::OpenLevel(this, GameWorldLevelPath);
}

void AContrarySurvivorPlayerController::TravelToBootLevel()
{
	// Возврат в главное меню из паузы: мир выгружается целиком, в памяти телефона от него
	// ничего не остаётся. Намерение обнуляем — решение снова за игроком.
	if (UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this))
	{
		Flow->SetWorldEntryIntent(EContraryWorldEntryIntent::None);
	}

	if (IsPaused())
	{
		SetPause(false);
	}

	UE_LOG(LogQA, Display, TEXT("QA: возвращаемся на загрузочный уровень '%s'"),
		*BootLevelPath.ToString());
	UGameplayStatics::OpenLevel(this, BootLevelPath);
}

void AContrarySurvivorPlayerController::HandleBootContinue()
{
	// «Продолжить» на загрузочном уровне: персонажа тут нет, поэтому сохранение не грузим —
	// его загрузит уже родившийся персонаж в мире, получив намерение «продолжить».
	CloseStartScreen();
	TravelToGameWorld(EContraryWorldEntryIntent::Continue);
}

void AContrarySurvivorPlayerController::HandleBootNewGame()
{
	// «Новая игра» на загрузочном уровне: персонажа нет, поэтому стираем слот сохранения
	// напрямую. Живые статы приводить к стартовым не нужно — персонаж в новом мире родится
	// заново и сам возьмёт значения новой игры, потому что сохранения уже не будет.
	CloseStartScreen();
	APlayerCharacter::DeleteDefaultSaveGame();
	TravelToGameWorld(EContraryWorldEntryIntent::NewGame);
}

// ---------------------------------------------------------------------------
// Главное меню (ADR-062, спека glavnoe-menu-spec.md; выросло из стартового экрана Б3):
// показывается со ВТОРОГО запуска игры (на загрузочном уровне — UpdateBootFlow, при запуске
// прямо на игровой карте — MaybeStartIntro). Паттерн — точная копия меню паузы
// (SetPause + барьер виджета), см. OpenPauseMenu/ClosePauseMenu выше.
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
			// Виджет не создался — не блокируем игру навсегда. На загрузочном уровне запускать
			// нечего (мира нет), поэтому просто везём игрока в мир новой игрой.
			UE_LOG(LogQA, Warning, TEXT("QA: start screen widget creation failed — falling back to new game"));
			if (bOnBootLevel)
			{
				TravelToGameWorld(EContraryWorldEntryIntent::NewGame);
			}
			else
			{
				StartNewGameFlow();
			}
			return;
		}
		StartScreenWidget->ApplyStyle(StartScreenStyle); // стиль с контроллера (EditAnywhere) поверх дефолтов
		// На загрузочном уровне персонажа нет, поэтому «Продолжить» и «Новая игра» ведут себя
		// иначе: сохранение не грузится и не стирается через персонажа, а в мир едет намерение.
		if (bOnBootLevel)
		{
			StartScreenWidget->OnContinueRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleBootContinue);
			StartScreenWidget->OnNewGameRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleBootNewGame);
		}
		else
		{
			StartScreenWidget->OnContinueRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleStartScreenContinue);
			StartScreenWidget->OnNewGameRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleStartScreenNewGame);
		}
		StartScreenWidget->OnExitRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleStartScreenExit);
		// Подход 2 волны меню: обработчик появился — и вместе с ним появился сам пункт
		// «Настройки». Виджет держит пункт спрятанным, пока OnSettingsRequested никем не
		// привязан (UStartScreenWidget::ApplyMenuRowVisibility), правок виджета не потребовалось.
		StartScreenWidget->OnSettingsRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleStartScreenSettings);
		// Задание издателя: пункт «Поддержать автора» между «Настройки» и «Сообщество».
		StartScreenWidget->OnSupportRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleMainMenuSupportRequested);
	}

	// Наличие сейва освежаем при КАЖДОМ открытии (меню без сейва: «Продолжить» не
	// показывается вовсе, «Новая игра» стартует без переспроса — спека).
	// ⛔ На загрузочном уровне персонажа нет вовсе, и спрашивать наличие сохранения у него
	// нельзя — иначе пункт «Продолжить» пропал бы у ВСЕХ. Там спрашиваем сам слот сохранения.
	APlayerCharacter* MenuPawn = Cast<APlayerCharacter>(GetPawn());
	const bool bMenuHasSave = MenuPawn ? MenuPawn->HasSaveGame() : APlayerCharacter::HasDefaultSaveGame();
	StartScreenWidget->SetHasSave(bMenuHasSave);

	// Z=70: выше меню паузы (60) и интро (50) — при интро экран не появляется, запас на будущее.
	StartScreenWidget->AddToViewport(/*ZOrder=*/70);

	bStartScreenOpen = true;
	bUIClickConsumed = false;

	// Игровой интерфейс убираем с экрана сразу, не дожидаясь следующего кадра HUD.
	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->ApplyMainMenuGateToStatsPanel();
	}
	ApplyWorldAudioGate(); // мир молчит, пока меню на экране, и звучит снова после него
	ApplyMenuMusicGate(); // музыка меню: играет и в паузе, и в главном меню

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
		bMenuHasSave ? TEXT("yes") : TEXT("no"));
}

bool AContrarySurvivorPlayerController::ShouldSilenceWorldSound(bool bIsUISound, bool bIsPlaying, bool bAlreadyPaused)
{
	// Звук интерфейса (щелчки кнопок, музыка меню) не трогаем никогда — иначе меню онемеет.
	// Молчащий звук глушить незачем. Приостановленный кем-то другим не наш: усыпили не мы,
	// значит и будить его при закрытии меню мы не вправе.
	return !bIsUISound && bIsPlaying && !bAlreadyPaused;
}

void AContrarySurvivorPlayerController::ApplyWorldAudioGate()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const bool bWantMuted = IsMainMenuOnScreen();

	// ЛЕСНОЙ ФОН — НАПРЯМУЮ ПО ССЫЛКЕ, а не через общий отбор ниже. Он заведён через
	// SpawnSound2D и потому считается звуком интерфейса, а общий отбор интерфейсные
	// пропускает намеренно. Живая сессия 08-09 показала ровно это: «найдено 1, заглушено 0,
	// пропущено: интерфейсных 1» — птицы пели поверх меню. Теперь фон глушится независимо
	// от признака, а общий отбор остаётся страховкой для будущих звуков мира.
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		if (PlayerChar->SetAmbienceSilenced(bWantMuted))
		{
			UE_LOG(LogQA, Display, TEXT("QA: лесной фон %s (меню на экране: %s) [ambience: silenced=%d]"),
				bWantMuted ? TEXT("приглушён") : TEXT("возвращён"),
				bWantMuted ? TEXT("да") : TEXT("нет"), bWantMuted ? 1 : 0);
		}
	}

	// Мир снова звучит: возвращаем к жизни ровно те звуки, что усыпили мы.
	if (!bWantMuted)
	{
		if (bWorldAudioMutedByMenu)
		{
			for (const TWeakObjectPtr<UAudioComponent>& Weak : MutedWorldSounds)
			{
				if (UAudioComponent* Sound = Weak.Get())
				{
					Sound->SetPaused(false);
				}
			}
			UE_LOG(LogQA, Display, TEXT("QA: звуки мира возвращены после меню (%d шт.)"),
				MutedWorldSounds.Num());
			MutedWorldSounds.Reset();
			bWorldAudioMutedByMenu = false;
		}
		return;
	}

	// Меню на экране. Проходим по миру не каждый кадр, а раз в секунду: звук мог начаться
	// уже при открытом меню (например, зациклённый эмбиент завёлся с задержкой).
	// Время живое (GetRealTimeSeconds) — игровое на паузе стоит, и проверка бы не повторялась.
	const double Now = World->GetRealTimeSeconds();
	if (bWorldAudioMutedByMenu && (Now - LastWorldAudioSweepTime) < 1.0)
	{
		return;
	}
	LastWorldAudioSweepTime = Now;

	int32 MutedNow = 0;
	int32 FoundInWorld = 0;
	int32 SkippedUi = 0;
	int32 SkippedSilent = 0;
	int32 SkippedAlreadyPaused = 0;
	for (TObjectIterator<UAudioComponent> It; It; ++It)
	{
		UAudioComponent* Sound = *It;
		if (!IsValid(Sound) || Sound->GetWorld() != World)
		{
			continue; // чужой мир (редактор, другой уровень) — не наше дело
		}
		++FoundInWorld;
		if (Sound->bIsUISound)
		{
			++SkippedUi;
		}
		else if (!Sound->IsPlaying())
		{
			++SkippedSilent;
		}
		else if (Sound->bIsPaused)
		{
			++SkippedAlreadyPaused;
		}
		if (!ShouldSilenceWorldSound(Sound->bIsUISound != 0, Sound->IsPlaying(), Sound->bIsPaused != 0))
		{
			continue;
		}
		Sound->SetPaused(true);
		MutedWorldSounds.Add(Sound);
		++MutedNow;
	}

	// Разбор ночной сессии 08-09: заглушение отработало, но НИ ОДНОГО звука не нашло
	// («звуки мира возвращены после меню (0 шт.)»), а птицы у Рината пели. Значит эмбиент
	// звучит не через аудиокомпонент этого мира — сам список кандидатов и говорит, почему.
	// Печатаем срез: сколько компонентов вообще нашлось в мире и по какой причине каждый
	// пропущен. Строка идёт только при СМЕНЕ картины, спама не будет.
	const FString Snapshot = FString::Printf(
		TEXT("найдено %d, заглушено %d, пропущено: интерфейсных %d, молчащих %d, уже на паузе %d"),
		FoundInWorld, MutedNow, SkippedUi, SkippedSilent, SkippedAlreadyPaused);
	if (!Snapshot.Equals(WorldAudioLastSnapshot))
	{
		WorldAudioLastSnapshot = Snapshot;
		UE_LOG(LogQA, Display,
			TEXT("QA: WORLD-AUDIO под меню: %s [world-audio: found=%d muted=%d ui=%d silent=%d paused=%d]"),
			*Snapshot, FoundInWorld, MutedNow, SkippedUi, SkippedSilent, SkippedAlreadyPaused);
	}
	bWorldAudioMutedByMenu = true;
}

void AContrarySurvivorPlayerController::CloseStartScreen()
{
	if (!bStartScreenOpen)
	{
		return;
	}
	bStartScreenOpen = false;
	bUIClickConsumed = false;

	// Возвращаем панель статов ТУТ ЖЕ. Ждать, что панель вернётся сама, нельзя: свернувший
	// себя виджет не тикает (регресс 08-09 — полос, голода, жажды и денег не было вовсе).
	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->ApplyMainMenuGateToStatsPanel();
	}
	ApplyWorldAudioGate(); // мир молчит, пока меню на экране, и звучит снова после него
	ApplyMenuMusicGate(); // музыка меню: играет и в паузе, и в главном меню

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

	// Настройки — часть «меню на экране»: гейт панели статов пересчитываем сразу.
	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->ApplyMainMenuGateToStatsPanel();
	}
	ApplyWorldAudioGate(); // мир молчит, пока меню на экране, и звучит снова после него
	ApplyMenuMusicGate(); // музыка меню: играет и в паузе, и в главном меню

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

	// Под настройками штатно остаётся главное меню — гейт пересчитываем, а не «включаем».
	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->ApplyMainMenuGateToStatsPanel();
	}
	ApplyWorldAudioGate(); // мир молчит, пока меню на экране, и звучит снова после него
	ApplyMenuMusicGate(); // музыка меню: играет и в паузе, и в главном меню

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
	else
	{
		// Загрузочный уровень: персонажа нет, поэтому стираем слот напрямую — иначе кнопка
		// «Стереть прогресс» в настройках, открытых из главного меню, не делала бы ничего.
		APlayerCharacter::DeleteDefaultSaveGame();
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

// ---------------------------------------------------------------------------
// Окно «Поддержать автора» (задание издателя, решение Рината 11.08.2026).
//
// Добровольная точка показа рекламы, куда игрок приходит САМ. Дословно из задания: «Ни один
// экран её не навязывает, она не прерывает игру и ничего не даёт взамен». Открывается ровно
// двумя пунктами — в главном меню и в меню паузы; окно при этом ОДНО на оба входа.
//
// ⛔ Три условия, которые нельзя нарушать:
//   • порог по игровому времени (правило РИ-29, AdGating::IsAdGatePassed) к этой точке НЕ
//     применяется и здесь не проверяется вовсе;
//   • за просмотр ролика игроку НИЧЕГО не начисляется;
//   • ролик не готов — кнопки просмотра нет вовсе, окно остаётся с одной кнопкой.
// ---------------------------------------------------------------------------

UClass* AContrarySurvivorPlayerController::ResolveSupportWidgetClass(UClass* AssignedClass)
{
	// ⛔ Запаску НЕ выбрасывать: пропал ассет или не заполнен слот — окно собирается кодом
	// (USupportAuthorWidget::BuildCodeTree), и игрок видит рабочее окно, а не пустоту.
	return AssignedClass ? AssignedClass : USupportAuthorWidget::StaticClass();
}

void AContrarySurvivorPlayerController::HandleMainMenuSupportRequested()
{
	OpenSupportScreen(UAnalyticsSubsystem::SupportSourceMainMenu());
}

void AContrarySurvivorPlayerController::HandlePauseSupportRequested()
{
	OpenSupportScreen(UAnalyticsSubsystem::SupportSourcePause());
}

void AContrarySurvivorPlayerController::OpenSupportScreen(const FString& Source)
{
	if (bSupportScreenOpen)
	{
		return;
	}

	if (!SupportScreenWidget)
	{
		// Слот назначен — окно из готового ассета; пуст — кодовое дерево-запаска.
		SupportScreenWidget = CreateWidget<USupportAuthorWidget>(this,
			ResolveSupportWidgetClass(SupportWidgetClass));
		if (!SupportScreenWidget)
		{
			// Окно не создалось — молча остаёмся там, где были. Игру не блокируем.
			UE_LOG(LogQA, Warning, TEXT("QA: окно «Поддержать автора» не создалось — остаёмся в меню"));
			return;
		}
		SupportScreenWidget->ApplyStyle(SupportScreenStyle);
		SupportScreenWidget->OnWatchAdRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleSupportWatchAd);
		SupportScreenWidget->OnSupportLinkRequested.AddUObject(this, &AContrarySurvivorPlayerController::HandleSupportLink);
		SupportScreenWidget->OnCloseRequested.AddUObject(this, &AContrarySurvivorPlayerController::CloseSupportScreen);
	}

	// Подсказка «новый ролик уже загружается» живёт от конца ролика до закрытия
	// окна. При открытии — сброс: без готового ролика окно обязано быть просто с одной кнопкой
	// (требование издателя), объяснение положено только сразу после просмотра.
	SupportScreenWidget->ResetNextAdHint();

	// Готовность ролика перечитываем при КАЖДОМ открытии: между показами окна ролик мог и
	// загрузиться, и разгрузиться. Порог по времени тут не спрашиваем намеренно.
	const IAdService* Ads = AdService::Get(this);
	const bool bAdReady = Ads && Ads->IsRewardedReady();
	SupportScreenWidget->SetAdAvailable(bAdReady);
	SupportScreenWidget->SetAdInProgress(false);

	// Z=78: выше экрана настроек (75) и главного меню (70), ниже экрана согласия (90).
	SupportScreenWidget->AddToViewport(/*ZOrder=*/78);

	bSupportScreenOpen = true;
	bUIClickConsumed = false;

	if (TouchControlsLayer)
	{
		TouchControlsLayer->SetLayerEnabled(false);
	}

	// Окно может открыться и поверх уже стоящей паузы (из меню), и из живой игры — во втором
	// случае паузу ставим сами и сами же снимаем. Чужую паузу не трогаем никогда.
	bPausedBySupportScreen = !IsPaused();
	if (bPausedBySupportScreen)
	{
		SetPause(true);
	}

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordSupportWindowOpened(Source);

		// Датчик «ролик предложен» (сводное ТЗ издателя 13.08, задача 3): ровно один раз на
		// открытие окна — дальше по этой функции пройти нельзя, пока окно не закроют
		// (bSupportScreenOpen в начале). Передаём ТУ ЖЕ готовность ролика, по которой только
		// что решили показывать кнопку, — датчик и кнопка разойтись не могут.
		Analytics->RecordSupportAdOffer(bAdReady);
	}
	UE_LOG(LogQA, Display, TEXT("QA: окно «Поддержать автора» открыто (откуда: %s, ролик готов: %s)"),
		*Source, bAdReady ? TEXT("да") : TEXT("нет"));
}

void AContrarySurvivorPlayerController::CloseSupportScreen()
{
	if (!bSupportScreenOpen)
	{
		return;
	}
	bSupportScreenOpen = false;
	bUIClickConsumed = false;

	if (SupportScreenWidget)
	{
		SupportScreenWidget->RemoveFromParent();
	}

	if (bPausedBySupportScreen)
	{
		SetPause(false);
		bPausedBySupportScreen = false;
	}

	// Слой экранных кнопок и игровой ввод возвращаем только если под окном не осталось другого
	// модального окна (штатно под ним лежит главное меню или пауза — они остаются открытыми).
	if (TouchControlsLayer && !IsAnyModalUIOpen())
	{
		TouchControlsLayer->SetLayerEnabled(true);
	}
	if (!IsAnyModalUIOpen())
	{
		SetInputMode(FInputModeGameOnly());
	}
	bShowMouseCursor = true;

	UE_LOG(LogQA, Display, TEXT("QA: окно «Поддержать автора» закрыто"));
}

void AContrarySurvivorPlayerController::HandleSupportWatchAd()
{
	IAdService* Ads = AdService::Get(this);
	if (!Ads || !Ads->IsRewardedReady())
	{
		// Ролик разгрузился между показом кнопки и нажатием — честно убираем кнопку.
		if (SupportScreenWidget)
		{
			SupportScreenWidget->SetAdAvailable(false);
		}
		UE_LOG(LogQA, Display, TEXT("QA: «Поддержать автора» — ролик не готов, кнопку убрали"));
		return;
	}

	if (SupportScreenWidget)
	{
		SupportScreenWidget->SetAdInProgress(true);
	}
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordSupportAdStarted();
	}
	UE_LOG(LogQA, Display, TEXT("QA: «Поддержать автора» — показываем ролик"));

	Ads->ShowRewarded(AdPlacements::SupportAuthor,
		FSimpleDelegate::CreateUObject(this, &AContrarySurvivorPlayerController::HandleSupportAdSuccess),
		FSimpleDelegate::CreateUObject(this, &AContrarySurvivorPlayerController::HandleSupportAdFail));
}

void AContrarySurvivorPlayerController::HandleSupportAdSuccess()
{
	if (SupportScreenWidget)
	{
		SupportScreenWidget->SetAdInProgress(false);
		// ⛔ НИКАКОЙ НАГРАДЫ: ни монет, ни предметов, ни бонусов. Дословно из задания —
		// «награда превращает её в обычную ежедневку и убивает замер». Всё, что происходит
		// после ролика, — короткое спасибо в этом же окне.
		SupportScreenWidget->ShowThanks();
		// Ролик мог разгрузиться после показа — освежаем видимость кнопки честно.
		const IAdService* Ads = AdService::Get(this);
		SupportScreenWidget->SetAdAvailable(Ads && Ads->IsRewardedReady());
	}

	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordSupportAdCompleted();
	}
	UE_LOG(LogQA, Display, TEXT("QA: «Поддержать автора» — ролик досмотрен, награды нет (так и задумано)"));
}

void AContrarySurvivorPlayerController::HandleSupportAdFail()
{
	// Задание: «Ролик упал с ошибкой после начала — показать то же спасибо». Игрок не должен
	// страдать из-за проблем рекламной сети. Событие «досмотрен» при этом НЕ шлём: замер
	// обязан считать реальные досмотры, а не наши утешения.
	if (SupportScreenWidget)
	{
		SupportScreenWidget->SetAdInProgress(false);
		SupportScreenWidget->ShowThanks();
		const IAdService* Ads = AdService::Get(this);
		SupportScreenWidget->SetAdAvailable(Ads && Ads->IsRewardedReady());
	}

	// Датчик «отказ от ролика» (сводное ТЗ издателя 13.08, задача 3). Сюда приходит ТОЛЬКО
	// показ без награды, то есть игрок закрыл ролик сам: технический сбой рекламная служба по
	// требованию издателя засчитывает как успех и уводит в ветку успеха, попутно отправляя своё
	// событие ad:support:failed (UYandexAdService: ShowFailed и сторож времени зовут
	// FinishShow с наградой). Поэтому отказ игрока и сбой техники в замере не смешиваются.
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordSupportAdDismissed();
	}
	UE_LOG(LogQA, Display, TEXT("QA: «Поддержать автора» — ролик не доиграл, показали то же спасибо"));
}

void AContrarySurvivorPlayerController::HandleSupportLink()
{
	// Адрес живёт ТОЛЬКО в настройках проекта, в коде его нет. Платёжных реквизитов в игре
	// нет ни в каком виде — всё это на внешней странице.
	const FString Url = UMainMenuSettings::GetSupportPostUrl();
	if (Url.IsEmpty())
	{
		// Пустую страницу игроку не показываем — тот же приём, что у политики и «Сообщества».
		UE_LOG(LogQA, Display,
			TEXT("QA: «Другие способы поддержать» — адрес в настройках не задан, открывать нечего"));
		return;
	}

	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordSupportLinkOpened();
	}

	FString Error;
	FPlatformProcess::LaunchURL(*Url, nullptr, &Error);
	UE_LOG(LogQA, Display, TEXT("QA: «Другие способы поддержать» — открыт адрес '%s'%s%s"),
		*Url, Error.IsEmpty() ? TEXT("") : TEXT(", ошибка: "), *Error);
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

// ---------------------------------------------------------------------------
// Дебаг-клавиши Рината (2026-08-14): G — скриншот, Y — статы 100%, U — заморозка врагов
// ---------------------------------------------------------------------------

namespace
{
	// Папка снимков на рабочем столе Windows. Стол Рината перенаправлен в OneDrive, поэтому
	// путь спрашиваем у системы: SHGetKnownFolderPath(FOLDERID_Desktop) учитывает
	// перенаправление, а USERPROFILE\Desktop дал бы «не тот» стол. Идиома вызова — как в
	// движке (WindowsPlatformProcess.cpp:1232: SHGetKnownFolderPath + CoTaskMemFree).
	FString GetDesktopScreenshotDir()
	{
#if PLATFORM_WINDOWS
		PWSTR RawPath = nullptr;
		FString Desktop;
		if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Desktop, 0, nullptr, &RawPath)) && RawPath)
		{
			Desktop = FString(RawPath);
		}
		if (RawPath)
		{
			CoTaskMemFree(RawPath);
		}
		return Desktop.IsEmpty() ? FString() : Desktop / TEXT("Скриншоты ContrarySurvivor");
#else
		return FString();
#endif
	}
}

void AContrarySurvivorPlayerController::OnQAScreenshot()
{
	// G: снимок ИГРОВОЙ ОБЛАСТИ (вьюпорт + интерфейс) в папку на рабочем столе. Само
	// снятие кадра — общий хелпер UContraryCheatManager::TakeViewportShot (тот же путь у
	// команды QAShot для витринных кадров): почему не RequestScreenshot(bShowUI) напрямую,
	// одноразовая подписка на OnScreenshotCaptured и запись PNG — объяснено там.
	FString Dir = GetDesktopScreenshotDir();
	if (Dir.IsEmpty())
	{
		Dir = FPaths::ScreenShotDir();
		FQADebug::QA(this, FString::Printf(
			TEXT("QA: SCREENSHOT desktop dir unavailable, fallback -> %s"), *Dir), /*bScreen=*/true);
	}

	const FString FilePath = Dir / FString::Printf(TEXT("shot_%s.png"),
		*FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));

	UContraryCheatManager::TakeViewportShot(GetWorld(), FilePath, TEXT("SCREENSHOT"));
}

void AContrarySurvivorPlayerController::OnQAFullStats()
{
	// Y: здоровье, голод и жажда игрока на максимум. Именно Set*-функции UStatsComponent
	// (SetHealth/SetHunger/SetThirst): они клампят значение и бродкастят делегаты
	// OnHealthChanged/OnHungerChanged/OnThirstChanged — HUD обновляется сам, в отличие от
	// прямой записи в поля.
	const APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	UStatsComponent* Stats = PlayerChar ? PlayerChar->GetStats() : nullptr;
	if (!Stats)
	{
		FQADebug::QA(this, TEXT("QA: FULL STATS skipped - no player stats"), /*bScreen=*/true);
		return;
	}

	Stats->SetHealth(Stats->GetMaxHealth());
	Stats->SetHunger(Stats->GetSurvivalMax());
	Stats->SetThirst(Stats->GetSurvivalMax());
	FQADebug::QA(this, TEXT("QA: FULL STATS (hp/hunger/thirst = max)"), /*bScreen=*/true);
}

void AContrarySurvivorPlayerController::OnQAToggleFreezeEnemies()
{
	// U: тумблер «заморозить/разморозить всех врагов». Тело — в ApplyQAFreezeEnemies:
	// его же зовёт явная команда QAFreezeEnemies 0|1 (съёмка витринных кадров).
	ApplyQAFreezeEnemies(!FQADebug::bFreezeEnemies);
}

void AContrarySurvivorPlayerController::ApplyQAFreezeEnemies(bool bFreeze)
{
	// Заморозить/разморозить всех врагов (волки + бандиты; игрок не трогается).
	// Мозги: state-machine AEnemyAIController гейтится флагом bFreezeEnemies прямо в Tick —
	// Behavior Tree у наших врагов НЕТ, BrainComponent обычно null (создаёт его только
	// RunBehaviorTree), поэтому PauseLogic зовём лишь при наличии мозга (задел на будущее).
	// Ноги: StopMovement (аборт активного MoveToActor) + StopMovementImmediately +
	// DisableMovement (MOVE_None глушит и path-following, и прямой ход AddMovementInput).
	// Разморозка: ResumeLogic (если был мозг) + SetMovementMode(MOVE_Walking).
	// Враг, заспавнившийся ПОСЛЕ заморозки, тоже стоит (глобальный флаг гейтит его Tick),
	// но его режим движения не трогался — разморозка ставит Walking всем без вреда.
	FQADebug::bFreezeEnemies = bFreeze;

	int32 Affected = 0;
	for (const TWeakObjectPtr<AEnemyAIController>& WeakCtrl : AEnemyAIController::GetActiveControllers())
	{
		AEnemyAIController* Ctrl = WeakCtrl.Get();
		ACharacter* EnemyChar = Ctrl ? Cast<ACharacter>(Ctrl->GetPawn()) : nullptr;
		UCharacterMovementComponent* Move = EnemyChar ? EnemyChar->GetCharacterMovement() : nullptr;
		if (!Move)
		{
			continue;
		}

		if (UBrainComponent* Brain = Ctrl->GetBrainComponent())
		{
			if (bFreeze)
			{
				Brain->PauseLogic(TEXT("QA freeze (U)"));
			}
			else
			{
				Brain->ResumeLogic(TEXT("QA freeze (U)"));
			}
		}

		if (bFreeze)
		{
			Ctrl->StopMovement();
			Move->StopMovementImmediately();
			Move->DisableMovement();
		}
		else
		{
			Move->SetMovementMode(MOVE_Walking);
		}
		++Affected;
	}

	FQADebug::QA(this, FString::Printf(TEXT("QA: FREEZE %s (enemies: %d)"),
		bFreeze ? TEXT("on") : TEXT("off"), Affected), /*bScreen=*/true);
}

void AContrarySurvivorPlayerController::OnQATogglePreviewShadows()
{
	// Period: тумблер надписей «Preview» в тенях непостроенного света (для съёмки скриншотов).
	// Откуда надписи (сверено по исходникам UE 5.5): DirectionalLight уровня — Stationary, а
	// запечённого света в проекте нет (ни одного *_BuiltData), поэтому рендерер проецирует
	// служебный материал PreviewShadowIndicatorMaterial на тени (LightRendering.cpp:1653:
	// ShowFlags.PreviewShadowsIndicator && !IsPrecomputedLightingValid && HasStaticShadowing).
	// Гасим штатной консольной переменной ShowFlag.PreviewShadowsIndicator
	// (SystemSettings.cpp:136: 0 — принудительно скрыть, 1 — принудительно показать,
	// 2 — не вмешиваться, умолчание). Применяется КАЖДЫЙ кадр: GameViewportClient.cpp:1463 →
	// EngineShowFlagOverride → маски Force0/Force1 из GSystemSettings (ShowFlags.cpp:689),
	// поэтому работает на лету и в PIE, и в отдельно запущенной игре. В Shipping сам флаг
	// вшит в ноль (ShowFlagsValues.inl:323) — там ни надписей, ни этой клавиши нет.
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ShowFlag.PreviewShadowsIndicator"));
	if (!CVar)
	{
		FQADebug::QA(this, TEXT("QA: PREVIEW LABELS cvar not found - skipped"), /*bScreen=*/true);
		return;
	}

	const bool bCurrentlyForcedOff = (CVar->GetInt() == 0);
	// Скрыто (0) -> вернуть умолчание движка (2); любое другое состояние -> скрыть (0).
	CVar->Set(bCurrentlyForcedOff ? 2 : 0, ECVF_SetByConsole);
	FQADebug::QA(this, FString::Printf(TEXT("QA: PREVIEW LABELS %s"),
		bCurrentlyForcedOff ? TEXT("restored (engine default)") : TEXT("hidden")), /*bScreen=*/true);
}

// ---------------------------------------------------------------------------
// T — god-mode (Фаза 5). Force-drop/spawn-wolf/overlay/force-kill/kill-player и весь
// QA-харнесс предметов, магазина и квестов УБРАНЫ 2026-08-15 по слову Рината (мешали съёмке).
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OnQAToggleGodMode()
{
	// T: тумблер неуязвимости + заморозки деградации голода/жажды. Экранную панель отладки
	// НЕ включает (2026-08-15: клавиши O для её выключения больше нет, а на кадрах для
	// магазина панель не нужна).
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
	FQADebug::QA(this, FString::Printf(TEXT("QA: GODMODE %s"), FQADebug::bGodMode ? TEXT("on") : TEXT("off")), /*bScreen=*/true);
}

// ---------------------------------------------------------------------------
// F1 — свободная камера, K — деньги
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OnToggleDebugCamera()
{
	// F1: переключение свободной камеры. Console-exec "ToggleDebugCamera" роутится в
	// UCheatManager::ToggleDebugCamera (ENGINE_API, UE 5.5): отвязывает камеру от игрока для
	// свободного облёта. Класс камеры задаёт наш UContraryCheatManager (CheatClass в
	// конструкторе) — AContraryDebugCameraController: без надписей/линий на экране, без
	// смены вьюмода (V), без буферов (B/Enter) и заморозки рендера (F); F1 внутри камеры —
	// возврат к игроку (сам движок клавиши выхода не даёт: ввод нашего контроллера при
	// активной debug-камере не обрабатывается, см. Debug/ContraryDebugCamera.h).
	ConsoleCommand(TEXT("ToggleDebugCamera"), /*bWriteToLog=*/true);
	UE_LOG(LogQA, Display, TEXT("QA: F1 ToggleDebugCamera"));
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
				// ADR-076 п.2 (решение лида 22.08 по кейсу Рината «в лагере два мешка,
				// обыскиваются по очереди»): якорь-МЕШОК тоже собирает ГРУППУ — соседние
				// мешки и тела уходят в одно окно. Группа пуста (мешок-«отдельное
				// хранилище» вернёт самого себя) — прежний одиночный путь.
				if (Pickup->UsesSearchWindow())
				{
					TArray<UCorpseLootComponent*> Group;
					for (const TWeakObjectPtr<UCorpseLootComponent>& Ptr : CurrentCorpseGroup)
					{
						if (UCorpseLootComponent* Member = Ptr.Get())
						{
							Group.Add(Member);
						}
					}
					if (Group.Num() == 0)
					{
						Group = UCorpseLootComponent::CollectSearchableGroup(
							Pickup, GetCorpseGroupSearchRadiusEffective());
					}
					if (Group.Num() > 0)
					{
						OpenCorpseLootGroup(Group);
					}
					else
					{
						OpenCorpseLoot(Pickup->GetLootContainer());
					}
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
			// Издатель 11.08.2026 п.3.1: одно нажатие открывает содержимое ВСЕХ необысканных
			// тел рядом, а не только ближайшего. Группа уже посчитана в Tick; если её нет
			// (вызов в обход подсказки), считаем прямо здесь от подсвеченного тела.
			TArray<UCorpseLootComponent*> Group;
			for (const TWeakObjectPtr<UCorpseLootComponent>& Ptr : CurrentCorpseGroup)
			{
				if (UCorpseLootComponent* Member = Ptr.Get())
				{
					Group.Add(Member);
				}
			}
			if (Group.Num() == 0)
			{
				Group = UCorpseLootComponent::CollectSearchableGroup(
					CurrentInteractActor, GetCorpseGroupSearchRadiusEffective());
			}
			OpenCorpseLootGroup(Group);
			break;
		}
		default:
			UE_LOG(LogTemp, Log, TEXT("Interact: nothing nearby"));
			break;
	}
}

float AContrarySurvivorPlayerController::GetCorpseGroupSearchRadiusEffective() const
{
	// ADR-076 п.2 (решение Рината): источник радиуса — BP ИГРОКА. Пешки нет (меню/переход) —
	// запасной путь: прежнее поле контроллера, чтобы поведение не менялось скачком.
	if (const APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		return PlayerChar->GetCorpseGroupSearchRadius();
	}
	return CorpseGroupSearchRadius;
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

		// ADR-082: торговля — тоже пара окон, тем же приёмом, что обыск. Справа открывается
		// связкой настоящий инвентарь в режиме Trade — оба окна обязаны существовать ДО
		// SetPanelMode (SetInventoryOpen создаёт/переиспользует UMG-экземпляр инвентаря).
		bInventoryOpen = true;
		CSHUD->SetInventoryOpen(true);
		if (UInventoryScreenWidget* InvWidget = CSHUD->GetInventoryWidgetInstance())
		{
			InvWidget->SetPanelMode(EItemPanelMode::Trade, CSHUD->GetShopWidgetInstance());
		}
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

		// ADR-082: закрываем и напарника — инвентарь возвращается в обычный режим.
		bInventoryOpen = false;
		CSHUD->SetInventoryOpen(false);
		if (UInventoryScreenWidget* InvWidget = CSHUD->GetInventoryWidgetInstance())
		{
			InvWidget->SetPanelMode(EItemPanelMode::Normal, nullptr);
		}
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = true;
	UE_LOG(LogTemp, Log, TEXT("Shop CLOSED"));
}

void AContrarySurvivorPlayerController::OpenCorpseLoot(UCorpseLootComponent* Corpse)
{
	// Одиночный контейнер (мешок-пикап и прежние вызовы) — группа из одного элемента.
	if (!Corpse)
	{
		return;
	}
	OpenCorpseLootGroup(TArray<UCorpseLootComponent*>({ Corpse }));
}

void AContrarySurvivorPlayerController::OpenCorpseLootGroup(const TArray<UCorpseLootComponent*>& InCorpses)
{
	// Build 1.2.1 (ТЗ А1): окно обыска трупа — модалка по образцу OpenShop. Издатель
	// 11.08.2026 п.3.1: ОДНО окно на всю группу тел, второй кнопки и второго окна нет.
	if (InCorpses.Num() == 0 || bCorpseLootOpen)
	{
		return;
	}

	bCorpseLootOpen = true;
	bUIClickConsumed = false;

	// ADR-082 п.6: пока окно открыто, тела/мешок группы не исчезают по таймеру — запоминаем и
	// останавливаем каждому живому контейнеру (остаток хранится в самом контейнере, см.
	// UCorpseLootComponent::PauseLifeSpanForSearchWindow). Список — чтобы CloseCorpseLoot
	// вернул таймер ровно тем же контейнерам, даже если сюда попадёт новая группа раньше.
	CurrentCorpseLootGroup.Reset();
	for (UCorpseLootComponent* Corpse : InCorpses)
	{
		if (IsValid(Corpse))
		{
			Corpse->PauseLifeSpanForSearchWindow();
			CurrentCorpseLootGroup.Add(Corpse);
		}
	}

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetCorpseLootGroupOpen(true, InCorpses);

		// ADR-082: обыск — это ПАРА окон, а не одно. Слева уже открылось окно обыска (выше),
		// справа связкой открывается инвентарь в режиме Search — оба окна обязаны существовать
		// ДО SetPanelMode (SetInventoryOpen создаёт/переиспользует UMG-экземпляр инвентаря).
		bInventoryOpen = true;
		CSHUD->SetInventoryOpen(true);
		if (UInventoryScreenWidget* InvWidget = CSHUD->GetInventoryWidgetInstance())
		{
			InvWidget->SetPanelMode(EItemPanelMode::Search, CSHUD->GetCorpseLootWidgetInstance());
		}
	}

	// Режим ввода — общий для пары окон, ставим один раз здесь (SetInventoryOpen сам его не
	// трогает, в отличие от самостоятельного открытия инвентаря клавишей Tab).
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;

	UE_LOG(LogQA, Display, TEXT("QA: CORPSE loot window OPEN ('%s', тел в группе: %d)"),
		*GetNameSafe(InCorpses[0] ? InCorpses[0]->GetOwner() : nullptr), InCorpses.Num());
}

void AContrarySurvivorPlayerController::CloseCorpseLoot()
{
	if (!bCorpseLootOpen)
	{
		return;
	}
	bCorpseLootOpen = false;
	bUIClickConsumed = false;

	// ADR-082 п.6: вернуть остановленный таймер исчезновения каждому контейнеру группы — кто
	// ещё жив (мог полностью раствориться/уйти в землю, пока окно было открыто).
	for (const TWeakObjectPtr<UCorpseLootComponent>& Ptr : CurrentCorpseLootGroup)
	{
		if (UCorpseLootComponent* Corpse = Ptr.Get())
		{
			Corpse->ResumeLifeSpanAfterSearchWindow();
		}
	}
	CurrentCorpseLootGroup.Reset();

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetCorpseLootOpen(false, nullptr);

		// ADR-082: закрываем и напарника — инвентарь возвращается в обычный режим (кнопка
		// закрытия рюкзака снова видна, клик по плитке снова применяет предмет).
		bInventoryOpen = false;
		CSHUD->SetInventoryOpen(false);
		if (UInventoryScreenWidget* InvWidget = CSHUD->GetInventoryWidgetInstance())
		{
			InvWidget->SetPanelMode(EItemPanelMode::Normal, nullptr);
		}
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = true;
	UE_LOG(LogQA, Display, TEXT("QA: CORPSE loot window CLOSED"));
}

void AContrarySurvivorPlayerController::OnToggleInventory()
{
	// ADR-082 п.2 (ТЗ издателя): «Если игрок нажмёт эту кнопку во время обыска, поверх
	// откроется второе окно инвентаря, и получится каша» — во время обыска/торговли кнопка
	// рюкзака и клавиша Tab не делают ничего. Экранная кнопка рюкзака идёт этим же путём
	// (TouchToggleInventory() зовёт этот метод), отдельно её трогать не нужно.
	if (bCorpseLootOpen || bShopOpen)
	{
		UE_LOG(LogQA, Display, TEXT("QA: OnToggleInventory проигнорирован — открыт %s"),
			bCorpseLootOpen ? TEXT("обыск") : TEXT("магазин"));
		return;
	}

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

	// Загрузочный уровень (ADR-067 п.5): мира нет, персонажа нет, интерфейса игры нет. Всё, что
	// контроллер обычно делает для игры — квесты, авто-цель, поиск интерактивов — здесь молчит,
	// поэтому дальше по функции мы просто не идём. Остаётся одно дело: принять решение и увезти
	// игрока либо в меню, либо сразу в мир.
	if (bOnBootLevel)
	{
		UpdateBootFlow();
		return;
	}

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

	// ADR-074: подсказка о сохранении у костра при первом выходе из деревни (один раз за профиль).
	UpdateLeaveVillageHint();

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

	// Переезд запуска (ADR-067 п.5): если игрок приехал сюда с загрузочного уровня, решение
	// уже принято ТАМ, и здесь мы ему просто подчиняемся. Заново спрашивать «был ли уже
	// запуск» нельзя: этот вопрос не читает признак, а ставит его, и самый первый запуск
	// после установки тихо сломался бы — игрок увидел бы меню поверх мира.
	EContraryWorldEntryIntent Intent = EContraryWorldEntryIntent::None;
	if (UGameFlowSubsystem* Flow = UGameFlowSubsystem::Get(this))
	{
		Intent = Flow->ConsumeWorldEntryIntent(); // намерение одноразовое, сразу сбрасываем
	}

	if (Intent == EContraryWorldEntryIntent::Continue)
	{
		// «Продолжить»: грузим сохранение целиком, вступление не играет (Б3, замечание 9).
		const bool bLoaded = PlayerChar->LoadGameForContinue();
		UE_LOG(LogQA, Display, TEXT("QA: приехали в мир по намерению «продолжить» (%s)"),
			bLoaded ? TEXT("сохранение загружено") : TEXT("загрузить не вышло — стартуем как есть"));
		if (bLoaded)
		{
			// Загрузились посреди этапа вступления — задача и стрелка обязаны вернуться.
			ResumeIntroObjectiveAfterContinue();
		}
		return;
	}

	if (Intent == EContraryWorldEntryIntent::NewGame)
	{
		// «Новая игра»: сохранение уже стёрто на загрузочном уровне, играем полное вступление.
		UE_LOG(LogQA, Display, TEXT("QA: приехали в мир по намерению «новая игра»"));
		StartNewGameFlow();
		return;
	}

	// ⛔ ЗАПАСНОЙ ПУТЬ: намерения нет. Так выглядит запуск прямо на игровой карте (кнопка Play
	// в редакторе — основной способ работы), и поведение здесь обязано остаться ровно прежним.
	//
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
	CurrentCorpseGroup.Reset();

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
	// Report1 баг 1 (Ринат: «базу можно обыскивать, как только умер последний заспавненный
	// враг») решён НЕ здесь, а составом группы: хранилище базы подтягивается в группу
	// якоря-трупа (решение лида 24.08, вариант А — CollectSearchableGroup), поэтому
	// подсветка честно остаётся за ближайшим объектом.
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

	// Группа под одно нажатие (издатель 11.08.2026 п.3.1 + ADR-076 п.2): считается ОТ
	// ПОДСВЕЧЕННОГО объекта, а не от игрока. Якорем может быть и ТЕЛО, и МЕШОК с окном
	// обыска (кейс Рината «в лагере два мешка») — мешки добираются реестром пикапов внутри
	// CollectSearchableGroup. Счёт «Обыскать (N)» берёт число отсюда (GetCorpseGroupCount).
	const APickup* AnchorPickup = (CurrentInteractKind == EInteractKind::Pickup)
		? Cast<APickup>(CurrentInteractActor) : nullptr;
	if (CurrentInteractKind == EInteractKind::Corpse || (AnchorPickup && AnchorPickup->UsesSearchWindow()))
	{
		for (UCorpseLootComponent* Member :
			UCorpseLootComponent::CollectSearchableGroup(CurrentInteractActor, GetCorpseGroupSearchRadiusEffective()))
		{
			CurrentCorpseGroup.Add(Member);
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

void AContrarySurvivorPlayerController::UpdateLeaveVillageHint()
{
	// Во время интро не проверяем: игрок начинает ЗА деревней и идёт к ней авто-подходом;
	// признак «был внутри» до конца интро не взводится, поэтому на старте подсказки нет.
	if (IntroPhase != EIntroPhase::None)
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		return;
	}

	// Граница деревни — та же, что для ИИ (единый источник, ADR-036): чистая математика по
	// боксу зоны, без коллизии, дёшево на каждый тик.
	const bool bInsideNow = AVillageZone::IsPointInVillage(World, ControlledPawn->GetActorLocation());
	if (bInsideNow)
	{
		bWasInsideVillage = true;
		return;
	}
	if (!bWasInsideVillage)
	{
		return; // снаружи с самого начала (старт игры) — это не «выход из деревни»
	}

	// Переход «внутри → снаружи». Признак сбрасываем, но подсказка всё равно одна на профиль:
	// одноразовость держит флаг в сейве (UOnboardingComponent::TryShowHint).
	bWasInsideVillage = false;
	if (UOnboardingComponent* OnboardingComp = GetOnboarding())
	{
		OnboardingComp->TryShowHint(EOnboardingHint::LeaveVillage);
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

FText AContrarySurvivorPlayerController::FormatCorpseGroupAction(const FText& Format,
	const FText& ActionText, int32 CorpseCount)
{
	// Дословно из задания (п.3.1): «Когда тело одно — просто Обыскать, без числа в скобках».
	if (CorpseCount <= 1 || ActionText.IsEmpty() || Format.IsEmpty())
	{
		return ActionText;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("Action"), ActionText);
	// Без разбивки разрядов: это счёт тел, а не денежная сумма.
	Args.Add(TEXT("Count"), FText::AsNumber(CorpseCount, &FNumberFormattingOptions::DefaultNoGrouping()));
	return FText::Format(Format, Args);
}

FText AContrarySurvivorPlayerController::GetInteractPromptDisplayText() const
{
	const APickup* NearPickup = (CurrentInteractKind == EInteractKind::Pickup)
		? Cast<APickup>(CurrentInteractActor) : nullptr;
	const bool bSearchWindow = NearPickup && NearPickup->UsesSearchWindow();

	FText ActionText = GetInteractActionText(CurrentInteractKind, bSearchWindow);

	// Групповой обыск (п.3.1): подсказка называет число тел — «Обыскать (4) — E». Мешка и
	// ящика это не касается: у них группы нет.
	if (CurrentInteractKind == EInteractKind::Corpse)
	{
		ActionText = FormatCorpseGroupAction(InteractPromptCorpseGroupFormat, ActionText, GetCorpseGroupCount());
	}

	return FormatInteractPrompt(InteractPromptFormat, ActionText, GetInteractHowText());
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
