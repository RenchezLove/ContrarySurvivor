// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivorPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ARangedWeapon.h"
#include "Engine/HitResult.h"
#include "EngineUtils.h" // TActorIterator
#include "ContrarySurvivor/HUD/ContrarySurvivorHUD.h"
#include "ContrarySurvivor/Actors/TraderNPC.h"
#include "ContrarySurvivor/Actors/Pickup.h"

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
	}
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

	// Контекстный interact (решение Рината/game-lead): действуем по БЛИЖАЙШЕМУ интерактиву,
	// выбранному в Tick (UpdateNearbyInteractable). Пикап -> подобрать, торговец -> магазин.
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
}

void AContrarySurvivorPlayerController::UpdateNearbyInteractable()
{
	CurrentInteractActor = nullptr;
	CurrentInteractKind = EInteractKind::None;

	// Пока открыт модальный экран — подсказку не предлагаем.
	if (bInventoryOpen || bShopOpen)
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
	// Пока открыт инвентарь/магазин — движение подавлено (модальный экран).
	if (bInventoryOpen || bShopOpen)
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
