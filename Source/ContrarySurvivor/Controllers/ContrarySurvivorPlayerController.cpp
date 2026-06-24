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
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"   // QA: спавн тест-волка (клавиша B)
#include "ContrarySurvivor/Subsystems/WolfSpawnSubsystem.h" // QA: телепорт к Логову (клавиша V)
#include "ContrarySurvivor/Subsystems/BanditSpawnSubsystem.h" // QA: телепорт к базе бандитов (клавиша Z)
#include "ContrarySurvivor/Subsystems/SpawnPlacementUtils.h" // QA: трасса до пола (телепорт V)
#include "ContrarySurvivor/Controllers/EnemyAIController.h" // QA headless-тест погони: режим/дальность
#include "NavigationSystem.h"  // QA headless-тест: проекция враг/игрок на навмеш (selfNav/targetNav)
#include "TimerManager.h"      // QA headless-тест: таймер-сэмплинг
#include "ContrarySurvivor/Debug/QADebug.h"              // QA debug-флаги/хелпер (J/U/B/O/N/V)
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Engine/Engine.h"                      // GEngine->Exec (подавление экранного спама)

AContrarySurvivorPlayerController::AContrarySurvivorPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentTarget = nullptr;
}

void AContrarySurvivorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

	// Показываем курсор мыши (нужен для выбора цели кликом)
	bShowMouseCursor = true;
	bEnableClickEvents = true;

	// QA-харнесс (Фаза 4 раунд 2): гасим экранные сообщения движка («LIGHTING NEEDS TO BE
	// REBUILT» и т.п.), чтобы не мешали приёмке автотестером. Это косметика рендера; сам
	// Build Lighting — позже. DisableAllScreenMessages выставляет GAreScreenMessagesEnabled=false
	// (подтверждено в UE 5.5: UEngine::HandleDisableAllScreenMessagesCommand).
	if (GEngine)
	{
		GEngine->Exec(GetWorld(), TEXT("DisableAllScreenMessages"));
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

		// QA debug-инструменты (Фаза 5, доп.): N — force-kill ближайшего врага; V — телепорт к Логову.
		InputComponent->BindAction(TEXT("QAForceKill"),     IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAForceKillNearest);
		InputComponent->BindAction(TEXT("QATeleportToDen"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnQATeleportToWolfDen);

		// Z — телепорт к базе бандитов (зеркало V/Логово): тестер не доходит на юг (−Y) пешком top-down камерой.
		InputComponent->BindAction(TEXT("QATeleportToBanditBase"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnQATeleportToBanditBase);

		// #26: возрождение по клавише (Enter / Пробел) на экране смерти — дубль кнопки «Возродиться».
		InputComponent->BindAction(TEXT("Respawn"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnRespawnPressed);

		// P (QA, #26): мгновенно убить игрока для теста экрана смерти.
		InputComponent->BindAction(TEXT("QAKillPlayer"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAKillPlayer);

		// Фаза 5: слайдер количества в магазине — ±количество (стрелки/колесо, Shift=±10).
		InputComponent->BindAction(TEXT("ShopQtyDec"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnShopQtyDec);
		InputComponent->BindAction(TEXT("ShopQtyInc"), IE_Pressed, this, &AContrarySurvivorPlayerController::OnShopQtyInc);
	}
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
	}
}

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

void AContrarySurvivorPlayerController::OnQATeleportToWolfDen()
{
	// V: телепорт игрока к Логову волков. Берём XY из UWolfSpawnSubsystem (источник истины —
	// тот же WolfDenLocation, по которому спавнятся волки), высоту — трассой до пола в этой XY.
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		FQADebug::QA(this, TEXT("QA: teleport to WolfDen skipped - no pawn/world"), /*bScreen=*/true);
		return;
	}

	UWolfSpawnSubsystem* WolfSys = World->GetSubsystem<UWolfSpawnSubsystem>();
	if (!WolfSys)
	{
		FQADebug::QA(this, TEXT("QA: teleport to WolfDen skipped - no WolfSpawnSubsystem"), /*bScreen=*/true);
		return;
	}

	const FVector Den = WolfSys->GetWolfDenLocation();

	// Высота капсулы игрока: садим капсулу на пол (трасса) + halfHeight, иначе безопасный Z.
	const ACharacter* AsChar = Cast<ACharacter>(ControlledPawn);
	const float HalfHeight = (AsChar && AsChar->GetCapsuleComponent())
		? AsChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const float SafeZ = SpawnPlacement::ResolveSpawnZ(
		World, Den.X, Den.Y, HalfHeight + 10.0f, TEXT("WolfDen-teleport"), ControlledPawn);

	const FVector Dest(Den.X, Den.Y, SafeZ);

	if (UCharacterMovementComponent* Move = AsChar ? AsChar->GetCharacterMovement() : nullptr)
	{
		Move->StopMovementImmediately();
	}
	ControlledPawn->SetActorLocation(Dest, /*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);

	FQADebug::QA(this, FString::Printf(TEXT("QA: teleported to WolfDen at %s"), *Dest.ToCompactString()), /*bScreen=*/true);
}

void AContrarySurvivorPlayerController::OnQATeleportToBanditBase()
{
	// Z: телепорт игрока к базе бандитов. Берём XY из UBanditSpawnSubsystem (источник истины —
	// тот же BanditBaseLocation, по которому активируется спавн бандитов «по приближению»),
	// высоту — трассой до пола в этой XY. Зеркало OnQATeleportToWolfDen (клавиша V).
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		FQADebug::QA(this, TEXT("QA: teleport to BanditBase skipped - no pawn/world"), /*bScreen=*/true);
		return;
	}

	UBanditSpawnSubsystem* BanditSys = World->GetSubsystem<UBanditSpawnSubsystem>();
	if (!BanditSys)
	{
		FQADebug::QA(this, TEXT("QA: teleport to BanditBase skipped - no BanditSpawnSubsystem"), /*bScreen=*/true);
		return;
	}

	const FVector Base = BanditSys->GetBanditBaseLocation();

	// Высота капсулы игрока: садим капсулу на пол (трасса) + halfHeight, иначе безопасный Z.
	const ACharacter* AsChar = Cast<ACharacter>(ControlledPawn);
	const float HalfHeight = (AsChar && AsChar->GetCapsuleComponent())
		? AsChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.0f;
	const float SafeZ = SpawnPlacement::ResolveSpawnZ(
		World, Base.X, Base.Y, HalfHeight + 10.0f, TEXT("BanditBase-teleport"), ControlledPawn);

	const FVector Dest(Base.X, Base.Y, SafeZ);

	if (UCharacterMovementComponent* Move = AsChar ? AsChar->GetCharacterMovement() : nullptr)
	{
		Move->StopMovementImmediately();
	}
	ControlledPawn->SetActorLocation(Dest, /*bSweep=*/false, /*OutSweepHitResult=*/nullptr, ETeleportType::TeleportPhysics);

	FQADebug::QA(this, FString::Printf(TEXT("QA: teleported to BanditBase at %s"), *Dest.ToCompactString()), /*bScreen=*/true);
}

// ---------------------------------------------------------------------------
// QA headless-автотест погони волков (cs.TestWolfChase, CU-free)
// ---------------------------------------------------------------------------
//
// НАЗНАЧЕНИЕ. Ночью live-PIE через Computer Use недоступен. Эта команда headless (-game)
// воспроизводит боевой сценарий и логирует ассерты QA-TEST: телепорт игрока к Логову (как
// клавиша V), принудительный спавн волков, затем 0.5с-сэмплинг ~15с с вердиктом PASS/FAIL.
//
// КРИТЕРИИ PASS (все три): (a) хотя бы один волк в Chase mode=nav (по решению самого
// AEnemyAIController, не по нашей реимплементации); (b) дистанция до ближайшего волка убывает
// монотонно (>=70% сэмплов фазы сближения «падают»); (c) контакт — dist<=эффективная дальность
// атаки (AttackRange+радиусы капсул, из кода врага) ИЛИ HP игрока упал.
//
// Дизайн-пороги НЕ выдуманы: AttackRange волка = 70 (AWolfAIController), эффективная дальность
// берётся методом GetEffectiveAttackRangeForQA() контроллера. Порог монотонности 70% — из ТЗ
// game-lead (повторяет логику chase-конвергенции AI).

void AContrarySurvivorPlayerController::QA_RunWolfChaseTest()
{
	UWorld* World = GetWorld();
	APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn());
	if (!World || !PlayerChar)
	{
		FQADebug::QA(this, TEXT("QA-TEST: WOLF-CHASE FAIL (no player pawn/world)"), /*bScreen=*/true);
		return;
	}

	// 1) Телепорт игрока к Логову — ПЕРЕИСПОЛЬЗУЕМ логику клавиши V (не дублируем).
	OnQATeleportToWolfDen();

	// 2) Принудительная активация Логова (спавн волков), как при приближении игрока.
	if (UWolfSpawnSubsystem* WolfSys = World->GetSubsystem<UWolfSpawnSubsystem>())
	{
		WolfSys->QAForceSpawnWolves();
	}
	else
	{
		FQADebug::QA(this, TEXT("QA-TEST: WOLF-CHASE FAIL (no WolfSpawnSubsystem)"), /*bScreen=*/true);
		return;
	}

	// 3) Базовые значения + запуск сэмплинга.
	QAChaseDistSamples.Reset();
	QAChaseSampleCount = 0;
	QAChaseAnyNavChase = false;
	QAChaseContact = false;
	QAChaseEffAttackRange = 0.0f;
	QAChaseStartTime = World->GetTimeSeconds();

	UStatsComponent* St = PlayerChar->GetStats();
	QAChaseStartPlayerHP = St ? St->GetHealth() : 0.0f;

	FQADebug::QA(this, FString::Printf(
		TEXT("QA-TEST: WOLF-CHASE START (player HP=%.0f, sampling 0.5s x30 ~15s)"),
		QAChaseStartPlayerHP), /*bScreen=*/true);

	// Зацикленный таймер сэмплинга (первый сэмпл через 0.5с — волкам нужен тик на агр/move).
	World->GetTimerManager().SetTimer(
		QAChaseTimerHandle, this, &AContrarySurvivorPlayerController::QA_WolfChaseSample,
		0.5f, /*bLoop=*/true, /*FirstDelay=*/0.5f);
}

void AContrarySurvivorPlayerController::QA_WolfChaseSample()
{
	UWorld* World = GetWorld();
	++QAChaseSampleCount;
	const float TElapsed = World ? (World->GetTimeSeconds() - (float)QAChaseStartTime) : 0.0f;

	APawn* PlayerPawn = GetPawn();
	const FVector PlayerLoc = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;

	// Ближайший ЖИВОЙ волк.
	AWolfCharacter* Nearest = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	if (World && PlayerPawn)
	{
		for (TActorIterator<AWolfCharacter> It(World); It; ++It)
		{
			AWolfCharacter* W = *It;
			if (!IsValid(W))
			{
				continue;
			}
			UStatsComponent* WS = W->FindComponentByClass<UStatsComponent>();
			if (WS && WS->IsDead())
			{
				continue;
			}
			const float D2 = FVector::DistSquared(PlayerLoc, W->GetActorLocation());
			if (D2 < BestDistSq)
			{
				BestDistSq = D2;
				Nearest = W;
			}
		}
	}

	float Dist = -1.0f;
	bool bSelfNav = false;
	bool bTargetNav = false;
	bool bNavChaseThis = false;

	if (Nearest)
	{
		Dist = FMath::Sqrt(BestDistSq);
		QAChaseDistSamples.Add(Dist);

		// Проекция враг/игрок на навмеш (для строки лога) — тот же подход, что в AI.Tick.
		if (UNavigationSystemV1* Nav = UNavigationSystemV1::GetCurrent(World))
		{
			const FVector QueryExtent(100.0f, 100.0f, 200.0f);
			FNavLocation Proj;
			bSelfNav = Nav->ProjectPointToNavigation(Nearest->GetActorLocation(), Proj, QueryExtent);
			if (PlayerPawn)
			{
				bTargetNav = Nav->ProjectPointToNavigation(PlayerLoc, Proj, QueryExtent);
			}
		}

		// Режим погони и эффективная дальность — из САМОГО контроллера врага (не реимплементация).
		if (AEnemyAIController* Ctrl = Cast<AEnemyAIController>(Nearest->GetController()))
		{
			if (Ctrl->IsChaseModeNavForQA())
			{
				bNavChaseThis = true;
				QAChaseAnyNavChase = true;
			}
			QAChaseEffAttackRange = Ctrl->GetEffectiveAttackRangeForQA(PlayerPawn);
		}

		// Контакт по дистанции.
		if (QAChaseEffAttackRange > 0.0f && Dist <= QAChaseEffAttackRange)
		{
			QAChaseContact = true;
		}
	}

	// Контакт по падению HP игрока (волк укусил).
	if (APlayerCharacter* PC = Cast<APlayerCharacter>(PlayerPawn))
	{
		if (UStatsComponent* St = PC->GetStats())
		{
			if (St->GetHealth() < QAChaseStartPlayerHP - 0.01f)
			{
				QAChaseContact = true;
			}
		}
	}
	else if (!PlayerPawn)
	{
		// Пешка игрока исчезла (вероятно, волки убили) — максимальное доказательство контакта.
		QAChaseContact = true;
	}

	FQADebug::QA(this, FString::Printf(
		TEXT("QA-TEST: t=%.1f nearestWolfDist=%.0f selfNav=%s targetNav=%s navChase=%s effRange=%.0f"),
		TElapsed, Dist,
		bSelfNav ? TEXT("yes") : TEXT("no"),
		bTargetNav ? TEXT("yes") : TEXT("no"),
		bNavChaseThis ? TEXT("yes") : TEXT("no"),
		QAChaseEffAttackRange), /*bScreen=*/true);

	// Завершение: контакт достигнут / исчерпан бюджет сэмплов (~15с) / пропала пешка игрока.
	if (QAChaseContact || QAChaseSampleCount >= 30 || !PlayerPawn)
	{
		QA_FinalizeWolfChaseTest();
	}
}

void AContrarySurvivorPlayerController::QA_FinalizeWolfChaseTest()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(QAChaseTimerHandle);
	}

	const int32 N = QAChaseDistSamples.Num();

	// Доля «сближающих» сэмплов: сосед-к-соседу падение дистанции больше эпсилон-шума (5 см,
	// как ChaseConvergeEpsilon в AI). Монотонность сближения = >=70% переходов «вниз».
	const float Epsilon = 5.0f;
	int32 Falling = 0;
	for (int32 i = 1; i < N; ++i)
	{
		if (QAChaseDistSamples[i] < QAChaseDistSamples[i - 1] - Epsilon)
		{
			++Falling;
		}
	}
	const float FallingRatio = (N > 1) ? ((float)Falling / (float)(N - 1)) : 0.0f;
	const bool bMonotonic = (N > 1) && (FallingRatio >= 0.70f);

	FQADebug::QA(this, FString::Printf(
		TEXT("QA-TEST: SUMMARY samples=%d fallingRatio=%.2f navChase=%s contact=%s effRange=%.0f"),
		N, FallingRatio,
		QAChaseAnyNavChase ? TEXT("yes") : TEXT("no"),
		QAChaseContact ? TEXT("yes") : TEXT("no"),
		QAChaseEffAttackRange), /*bScreen=*/true);

	if (QAChaseAnyNavChase && bMonotonic && QAChaseContact)
	{
		FQADebug::QA(this, TEXT("QA-TEST: WOLF-CHASE PASS"), /*bScreen=*/true);
	}
	else
	{
		FString Reason;
		if (N == 0)
		{
			Reason = TEXT("no-wolves-sampled");
		}
		else
		{
			if (!QAChaseAnyNavChase) { Reason += TEXT("no-nav-chase; "); }
			if (!bMonotonic)         { Reason += TEXT("dist-not-monotonic; "); }
			if (!QAChaseContact)     { Reason += TEXT("no-contact; "); }
		}
		FQADebug::QA(this, FString::Printf(TEXT("QA-TEST: WOLF-CHASE FAIL (%s)"), *Reason), /*bScreen=*/true);
	}
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
		UE_LOG(LogQA, Display, TEXT("QA: F3 EquipTestArmor"));
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
	}

	bDialogOpen = true;
	bUIClickConsumed = false;

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetDialogOpen(true, Elder);
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
}

void AContrarySurvivorPlayerController::CloseAllUI()
{
	// Магазин/диалог: их Close* сами синхронизируют HUD-флаг и возвращают режим ввода в Game
	// (early-return, если окно не открыто).
	CloseShop();
	CloseDialog();

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
				const bool bOk = Pickup->Collect(PlayerChar);
				UE_LOG(LogTemp, Log, TEXT("Interact: pickup collect %s"), bOk ? TEXT("OK") : TEXT("FAIL"));
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

	if (AContrarySurvivorHUD* CSHUD = GetHUD<AContrarySurvivorHUD>())
	{
		CSHUD->SetShopOpen(false, nullptr);
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = true;
	UE_LOG(LogTemp, Log, TEXT("Shop CLOSED"));
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
}

void AContrarySurvivorPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

void AContrarySurvivorPlayerController::UpdateNearbyInteractable()
{
	CurrentInteractActor = nullptr;
	CurrentInteractKind = EInteractKind::None;

	// Пока открыт модальный экран (вкл. экран смерти) — подсказку не предлагаем.
	if (bInventoryOpen || bShopOpen || bDialogOpen || bDeathScreen)
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
}

bool AContrarySurvivorPlayerController::HasInteractPrompt() const
{
	return CurrentInteractKind != EInteractKind::None && IsValid(CurrentInteractActor);
}

FString AContrarySurvivorPlayerController::GetInteractPromptText() const
{
	switch (CurrentInteractKind)
	{
		case EInteractKind::Pickup: return TEXT("E — подобрать");
		case EInteractKind::Trader: return TEXT("E — торговать");
		case EInteractKind::Elder:  return TEXT("E — поговорить");
		default:                    return FString();
	}
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
	// Пока открыт инвентарь/магазин/диалог/экран смерти — движение подавлено (модальный экран).
	if (bInventoryOpen || bShopOpen || bDialogOpen || bDeathScreen)
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
