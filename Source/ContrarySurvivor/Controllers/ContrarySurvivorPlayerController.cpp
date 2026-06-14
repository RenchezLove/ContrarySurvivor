// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivorPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ARangedWeapon.h"
#include "UInventoryComponent.h"      // QA-харнесс: рюкзак (использовать/выбросить/продать)
#include "AMasterInventoryItem.h"     // QA-харнесс: предмет + EItemCategory
#include "Kismet/GameplayStatics.h"   // QA-харнесс: DeleteGameInSlot (очистка сейва)
#include "Engine/HitResult.h"
#include "EngineUtils.h" // TActorIterator
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h"
#include "ContrarySurvivor/Actors/TraderNPC.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"           // Фаза 5: староста (диалог/квест)
#include "ContrarySurvivor/Components/QuestComponent.h"  // Фаза 5: журнал квестов игрока
#include "ContrarySurvivor/Actors/Pickup.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"   // QA: спавн тест-волка (клавиша B)
#include "ContrarySurvivor/Debug/QADebug.h"              // QA debug-флаги/хелпер (J/U/B/O)
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

		// QA debug-инструменты (Фаза 5): god/forcedrop/spawn-wolf/overlay (J/U/B/O), legacy ActionMapping.
		InputComponent->BindAction(TEXT("QAGodMode"),      IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAToggleGodMode);
		InputComponent->BindAction(TEXT("QAForceDrop"),    IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAToggleForceDrop);
		InputComponent->BindAction(TEXT("QASpawnWolf"),    IE_Pressed, this, &AContrarySurvivorPlayerController::OnQASpawnTestWolf);
		InputComponent->BindAction(TEXT("QAToggleOverlay"),IE_Pressed, this, &AContrarySurvivorPlayerController::OnQAToggleOverlay);
	}
}

// ---------------------------------------------------------------------------
// QA debug-инструменты (Фаза 5): god-mode / force-drop / spawn-wolf / overlay
// ---------------------------------------------------------------------------

void AContrarySurvivorPlayerController::OnQAToggleGodMode()
{
	// J: тумблер неуязвимости + заморозки деградации голода/жажды. Включаем оверлей вместе
	// с god-mode, чтобы тестер сразу видел статус на экране.
	FQADebug::bGodMode = !FQADebug::bGodMode;
	if (FQADebug::bGodMode)
	{
		FQADebug::bOverlayVisible = true;
	}
	FQADebug::QA(this, FString::Printf(TEXT("QA: GODMODE %s"), FQADebug::bGodMode ? TEXT("on") : TEXT("off")));
}

void AContrarySurvivorPlayerController::OnQAToggleForceDrop()
{
	// U: тумблер 100%-дропа со всех врагов.
	FQADebug::bForceDrop = !FQADebug::bForceDrop;
	FQADebug::QA(this, FString::Printf(TEXT("QA: FORCEDROP %s"), FQADebug::bForceDrop ? TEXT("on") : TEXT("off")));
}

void AContrarySurvivorPlayerController::OnQASpawnTestWolf()
{
	// B: заспавнить одного тест-волка чуть впереди игрока (быстро убить и проверить лут).
	APawn* ControlledPawn = GetPawn();
	UWorld* World = GetWorld();
	if (!ControlledPawn || !World)
	{
		FQADebug::QA(this, TEXT("QA: spawn test wolf skipped - no pawn/world"));
		return;
	}

	const FVector SpawnLoc = ControlledPawn->GetActorLocation()
		+ ControlledPawn->GetActorForwardVector() * 300.0f
		+ FVector(0.0f, 0.0f, 90.0f);
	const FRotator SpawnRot = (ControlledPawn->GetActorLocation() - SpawnLoc).Rotation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AWolfCharacter* Wolf = World->SpawnActor<AWolfCharacter>(
		AWolfCharacter::StaticClass(), SpawnLoc, FRotator(0.0f, SpawnRot.Yaw, 0.0f), SpawnParams);

	FQADebug::QA(this, Wolf
		? FString::Printf(TEXT("QA: spawned test wolf %s"), *Wolf->GetName())
		: TEXT("QA: spawned test wolf FAILED"));
}

void AContrarySurvivorPlayerController::OnQAToggleOverlay()
{
	// O: тумблер видимости экранного QA-оверлея.
	FQADebug::bOverlayVisible = !FQADebug::bOverlayVisible;
	FQADebug::QA(this, FString::Printf(TEXT("QA: overlay %s"), FQADebug::bOverlayVisible ? TEXT("on") : TEXT("off")));
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

	ATraderNPC* Trader = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	const FVector PawnLoc = ControlledPawn->GetActorLocation();
	for (TActorIterator<ATraderNPC> It(World); It; ++It)
	{
		ATraderNPC* Candidate = *It;
		if (!IsValid(Candidate))
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
	if (!IsValid(NearbyTrader))
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
	if (!IsValid(NearbyTrader))
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

	const FQuest& Offered = NearbyElder->GetOfferedQuest();
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

	const FQuest& Offered = NearbyElder->GetOfferedQuest();
	if (!PlayerQuests->TurnInQuest(Offered.QuestId)) // TURNED IN (или skip, если не Completed)
	{
		UE_LOG(LogQA, Display, TEXT("QA: turn-in skipped - quest not completed"));
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

void AContrarySurvivorPlayerController::SetNearbyTrader(ATraderNPC* Trader)
{
	NearbyTrader = Trader;
}

void AContrarySurvivorPlayerController::ClearNearbyTrader(ATraderNPC* Trader)
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

	// Предлагаем квест старосты журналу игрока (идемпотентно; OFFERED логируется один раз).
	if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(GetPawn()))
	{
		if (UQuestComponent* PlayerQuests = PlayerChar->GetQuests())
		{
			PlayerQuests->OfferQuest(Elder->GetOfferedQuest());
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
			if (ATraderNPC* Trader = Cast<ATraderNPC>(CurrentInteractActor))
			{
				OpenShop(Trader);
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

void AContrarySurvivorPlayerController::OpenShop(ATraderNPC* Trader)
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

	UE_LOG(LogTemp, Log, TEXT("Shop OPEN (trader %s)"), *Trader->GetName());
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

	// Пока открыт модальный экран — подсказку не предлагаем.
	if (bInventoryOpen || bShopOpen || bDialogOpen)
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

	// Торговец: проксимити уже задана его overlap-триггером (NearbyTrader).
	if (IsValid(NearbyTrader))
	{
		BestDistSq = FVector::DistSquared(Loc, NearbyTrader->GetActorLocation());
		CurrentInteractActor = NearbyTrader;
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
	// Пока открыт инвентарь/магазин/диалог — движение подавлено (модальный экран).
	if (bInventoryOpen || bShopOpen || bDialogOpen)
	{
		return;
	}

	FVector2D MovementVector = Value.Get<FVector2D>();

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	APlayerCameraManager* CameraManager = GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
	FRotator CameraRot = CameraManager->GetCameraRotation();
	FRotator FlatRot(0.0f, CameraRot.Yaw, 0.0f);

	FVector ScreenForward = FlatRot.Vector();
	FVector ScreenRight = FlatRot.RotateVector(FVector::RightVector);

	ScreenForward.Z = 0.0f;
	ScreenRight.Z = 0.0f;
	ScreenForward = ScreenForward.GetSafeNormal();
	ScreenRight = ScreenRight.GetSafeNormal();

	ControlledPawn->AddMovementInput(ScreenForward, MovementVector.Y);
	ControlledPawn->AddMovementInput(ScreenRight, MovementVector.X);
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
