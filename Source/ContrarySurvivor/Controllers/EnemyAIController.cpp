// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyAIController.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h" // D6: пистолет в руке (GetCurrentWeapon)
#include "ContrarySurvivor/Actors/VillageZone.h" // D7: граница деревни (оба режима погони)
#include "ARangedWeapon.h" // D6: PlayFireVisuals (след пули/вспышка/звук выстрела бандита)
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h" // замедление у границы деревни
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "Navigation/PathFollowingComponent.h" // EPathFollowingStatus (GetMoveStatus в Chase), EPathFollowingRequestResult
#include "NavigationSystem.h" // UNavigationSystemV1::GetCurrent / ProjectPointToNavigation + FNavLocation (QA-диагностика навмеша)
#include "ContrarySurvivor/Debug/QADebug.h" // QA-лог погони (дросселированный)
#include "ContrarySurvivor/Navigation/NavQueryFilter_ExcludeVillage.h" // BugReport 12: обход деревни

TArray<TWeakObjectPtr<AEnemyAIController>> AEnemyAIController::ActiveControllers;

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	// До первого реального MoveToActor считаем запрос непроведённым (Failed).
	LastMoveResult = EPathFollowingRequestResult::Failed;

	// BugReport 12: путь врага (бандит/волк) ИСКЛЮЧАЕТ зону деревни (UNavArea_Village).
	// Дефолтный фильтр; можно переопределить в BP-контроллере. Передаётся в MoveToActor.
	MoveFilterClass = UNavQueryFilter_ExcludeVillage::StaticClass();
}

void AEnemyAIController::BeginPlay()
{
	Super::BeginPlay();
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Берём UStatsComponent обобщённо через FindComponentByClass — контроллер подходит
	// любому врагу с компонентом статов (бандит-гуманоид И волк-квадрупед), без жёсткой
	// привязки к AEnemyCharacter (Фаза 3: переиспользование AI для волка).
	OwnStats = InPawn ? InPawn->FindComponentByClass<UStatsComponent>() : nullptr;

	CurrentState = EEnemyAIState::Idle;

	// Реестр живых контроллеров (лимит атакующих ADR-037 + боевая камера ADR-035).
	ActiveControllers.AddUnique(this);

	// Кэш скорости сбрасываем: пешка выставит свою MaxWalkSpeed в BeginPlay, кэшируем в Tick.
	CachedBaseWalkSpeed = -1.0f;
	bVillageSlowdownActive = false;

	// Поводок по умолчанию (ADR-036): дом = точка спавна/размещения. Спавнер базы
	// (AMasterEnemyBase) переопределит SetLeash'ем сразу после спавна (центр базы + её радиус).
	if (!bLeashSet && InPawn)
	{
		HomeLocation = InPawn->GetActorLocation();
		LeashRadius = FMath::Max(0.0f, DefaultLeashRadius);
		bLeashSet = true;
	}
}

void AEnemyAIController::OnUnPossess()
{
	// Восстановить скорость пешки, если уходили в замедление у деревни (пешка может пережить контроллер).
	SetVillageSlowdown(false);
	ActiveControllers.Remove(this);
	Super::OnUnPossess();
}

void AEnemyAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveControllers.Remove(this);
	Super::EndPlay(EndPlayReason);
}

void AEnemyAIController::SetLeash(const FVector& InHomeLocation, float InLeashRadius)
{
	HomeLocation = InHomeLocation;
	LeashRadius = FMath::Max(0.0f, InLeashRadius);
	bLeashSet = true;
}

APawn* AEnemyAIController::GetPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

bool AEnemyAIController::CanSensePlayer(APawn* Player) const
{
	APawn* Self = GetPawn();
	if (!Player || !Self)
	{
		return false;
	}

	const float Dist = Self->GetDistanceTo(Player);
	if (Dist > DetectionRange)
	{
		return false;
	}

	// LineOfSightTo унаследован от AController — учитывает препятствия.
	return LineOfSightTo(Player);
}

float AEnemyAIController::GetCombinedCapsuleRadius(APawn* Player) const
{
	float Combined = 0.0f;

	if (const ACharacter* SelfChar = Cast<ACharacter>(GetPawn()))
	{
		if (const UCapsuleComponent* Capsule = SelfChar->GetCapsuleComponent())
		{
			Combined += Capsule->GetScaledCapsuleRadius();
		}
	}

	if (const ACharacter* PlayerChar = Cast<ACharacter>(Player))
	{
		if (const UCapsuleComponent* Capsule = PlayerChar->GetCapsuleComponent())
		{
			Combined += Capsule->GetScaledCapsuleRadius();
		}
	}

	return Combined;
}

bool AEnemyAIController::PerformAttack(APawn* Player)
{
	if (!Player)
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < AttackCooldown)
	{
		return false; // ещё на кулдауне
	}

	LastAttackTime = Now;

	// Вариант A прицеливания: гуманоид-носитель плавно доворачивается корпусом на игрока и при
	// ближнем ударе (SetFocus пешку НЕ вращает: у ACharacter bUseControllerRotation* по умолчанию
	// false, FaceRotation при них no-op). Волк — не гуманоид, Cast даёт nullptr (его не трогаем).
	if (AMasterHumanoidCharacter* SelfHumanoid = Cast<AMasterHumanoidCharacter>(GetPawn()))
	{
		SelfHumanoid->StartAimTurnTo(Player);
	}

	// Урон игроку через стандартный пайплайн UE.
	FDamageEvent DamageEvent;
	Player->TakeDamage(AttackDamage, DamageEvent, this, GetPawn());

	UE_LOG(LogTemp, Log, TEXT("%s attacks player for %.1f"),
		GetPawn() ? *GetPawn()->GetName() : TEXT("Enemy"), AttackDamage);

	return true;
}

bool AEnemyAIController::PerformRangedAttack(APawn* Player)
{
	APawn* Self = GetPawn();
	if (!Player || !Self)
	{
		return false;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastRangedAttackTime < RangedAttackCooldown)
	{
		return false; // на кулдауне
	}

	// «Правило честности» (ADR-035): вне кадра камеры игрока не стреляем. Основной гейт стоит
	// в Tick (там враг вместо выстрела сближается); здесь — страховка от прямых вызовов.
	if (bHonorCameraFairness && !IsSelfOnPlayerScreen())
	{
		return false;
	}

	LastRangedAttackTime = Now;

	// Вариант A прицеливания (фидбек Рината 07-05): бандит плавно доворачивается корпусом на
	// игрока при реальном выстреле (кулдаун/честность уже пройдены). До этого корпус бандита
	// НИЧТО не вращало: SetFocus при выключенных bUseControllerRotation* пешку не поворачивает,
	// bOrientRotationToMovement у бандита тоже выключен — стрелял «из любой позы».
	if (AMasterHumanoidCharacter* SelfHumanoid = Cast<AMasterHumanoidCharacter>(Self))
	{
		SelfHumanoid->StartAimTurnTo(Player);
	}

	// Разброс как вероятность попадания (дешевле честной баллистики; Android-бюджет).
	const bool bHit = FMath::FRand() <= RangedHitChance;

	const FVector TargetLoc = Player->GetActorLocation();
	FVector TraceEnd = TargetLoc;
	if (!bHit)
	{
		// Промах: точка уходит вбок от линии выстрела (след пули летит мимо игрока).
		const FVector Dir = (TargetLoc - Self->GetActorLocation()).GetSafeNormal2D();
		const FVector Perp(-Dir.Y, Dir.X, 0.0f);
		TraceEnd += Perp * RangedMissOffset * (FMath::RandBool() ? 1.0f : -1.0f)
			+ FVector(0.0f, 0.0f, FMath::FRandRange(-20.0f, 40.0f));
	}
	else
	{
		// Урон штатным пайплайном (у игрока сработают броня/звук боли/тряска камеры).
		FDamageEvent DamageEvent;
		Player->TakeDamage(RangedAttackDamage, DamageEvent, this, Self);
	}

	// Визуал + звук выстрела — через пистолет в руке пешки (D1/D2: тот же SM_Pistol и
	// pistol_22_gunshot, что у игрока). Если оружия нет — урон уже нанесён, но громко логируем.
	ARangedWeapon* Weapon = nullptr;
	if (const AMasterHumanoidCharacter* Humanoid = Cast<AMasterHumanoidCharacter>(Self))
	{
		Weapon = Cast<ARangedWeapon>(Humanoid->GetCurrentWeapon());
	}
	if (Weapon)
	{
		Weapon->PlayFireVisuals(TraceEnd, /*bPlaySound=*/true);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: ranged attack without ranged weapon in hand (no visuals). Check SidearmWeaponClass."),
			*Self->GetName());
	}

	UE_LOG(LogTemp, Log, TEXT("%s shoots player: %s (dmg %.1f, chance %.2f)"),
		*Self->GetName(), bHit ? TEXT("HIT") : TEXT("miss"), RangedAttackDamage, RangedHitChance);

	return true;
}

bool AEnemyAIController::IsSelfOnPlayerScreen() const
{
	const APawn* Self = GetPawn();
	if (!Self)
	{
		return false;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		return false; // нет камеры игрока — стрелять не по кому
	}

	int32 SizeX = 0;
	int32 SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	if (SizeX <= 0 || SizeY <= 0)
	{
		// Вьюпорта нет (headless-прогон) — правило честности не блокирует бой.
		return true;
	}

	FVector2D ScreenPos;
	if (!PC->ProjectWorldLocationToScreen(Self->GetActorLocation(), ScreenPos, /*bPlayerViewportRelative=*/true))
	{
		return false; // за камерой
	}

	return ScreenPos.X >= 0.0f && ScreenPos.X <= static_cast<float>(SizeX)
		&& ScreenPos.Y >= 0.0f && ScreenPos.Y <= static_cast<float>(SizeY);
}

bool AEnemyAIController::HasAttackSlot(APawn* Player) const
{
	if (MaxSimultaneousAttackers <= 0 || !Player)
	{
		return true;
	}

	const APawn* Self = GetPawn();
	if (!Self)
	{
		return false;
	}

	// ADR-037: слоты достаются БЛИЖАЙШИМ к игроку. Считаем врагов, которые УЖЕ ведут бой
	// (Chase/Attack) и ближе нас; если таких >= лимита — нам слот не положен (Standoff).
	// Эпсилон 1 см рвёт равенство дистанций (двое на одинаковом расстоянии не мигают).
	const float MyDist = Self->GetDistanceTo(Player);
	int32 CloserEngaged = 0;

	for (const TWeakObjectPtr<AEnemyAIController>& Ptr : ActiveControllers)
	{
		const AEnemyAIController* Other = Ptr.Get();
		if (!Other || Other == this || Other->GetWorld() != GetWorld())
		{
			continue;
		}
		const APawn* OtherPawn = Other->GetPawn();
		if (!OtherPawn || (Other->OwnStats && Other->OwnStats->IsDead()))
		{
			continue;
		}
		if (Other->CurrentState != EEnemyAIState::Chase && Other->CurrentState != EEnemyAIState::Attack)
		{
			continue;
		}
		if (OtherPawn->GetDistanceTo(Player) < MyDist - 1.0f)
		{
			++CloserEngaged;
		}
	}

	return CloserEngaged < MaxSimultaneousAttackers;
}

bool AEnemyAIController::IsAheadInVillage(const FVector& Dir, float Dist) const
{
	const APawn* Self = GetPawn();
	if (!Self || !bRespectVillageSafeZone)
	{
		return false;
	}
	return AVillageZone::IsPointInVillage(GetWorld(), Self->GetActorLocation() + Dir * Dist);
}

void AEnemyAIController::SetVillageSlowdown(bool bSlow)
{
	if (bSlow == bVillageSlowdownActive)
	{
		return;
	}

	ACharacter* SelfChar = Cast<ACharacter>(GetPawn());
	UCharacterMovementComponent* Move = SelfChar ? SelfChar->GetCharacterMovement() : nullptr;
	if (!Move || CachedBaseWalkSpeed <= 0.0f)
	{
		return; // базовая скорость ещё не кэширована — не портим MaxWalkSpeed
	}

	bVillageSlowdownActive = bSlow;
	Move->MaxWalkSpeed = bSlow ? CachedBaseWalkSpeed * VillageSlowdownSpeedFactor : CachedBaseWalkSpeed;
}

void AEnemyAIController::StartReturnHome(const TCHAR* Reason)
{
	StopMovement();
	ClearFocus(EAIFocusPriority::Gameplay);
	SetVillageSlowdown(false);

	CurrentState = EEnemyAIState::Return;

	// Переиспользуем трекеры move-запросов погони (Chase и Return взаимоисключающи;
	// вход в Chase сбрасывает их заново).
	LastMoveIssueTime = -1000.0f;
	LastMoveResult = EPathFollowingRequestResult::RequestSuccessful;

	if (const APawn* Self = GetPawn())
	{
		FQADebug::QA(GetWorld(), FString::Printf(
			TEXT("QA: %s return home (%s) distHome=%.0f"),
			*Self->GetName(), Reason,
			FVector::DistXY(Self->GetActorLocation(), HomeLocation)), /*bScreen=*/true);
	}
}

void AEnemyAIController::TickReturnHome(float Now)
{
	APawn* Self = GetPawn();
	if (!Self)
	{
		return;
	}

	const float DistHome = FVector::DistXY(Self->GetActorLocation(), HomeLocation);
	if (DistHome <= ReturnAcceptRadius)
	{
		StopMovement();
		CurrentState = EEnemyAIState::Idle;
		FQADebug::QA(GetWorld(), FString::Printf(
			TEXT("QA: %s reached home (dist=%.0f)"), *Self->GetName(), DistHome), /*bScreen=*/true);
		return;
	}

	// Последний запрос пути домой упал (навмеша нет/не готов) — прямой ход (fallback,
	// та же дыра, что в погоне: возврат обязан работать и без навмеша).
	if (LastMoveResult == EPathFollowingRequestResult::Failed)
	{
		const FVector Dir = (HomeLocation - Self->GetActorLocation()).GetSafeNormal2D();
		if (!Dir.IsNearlyZero())
		{
			Self->AddMovementInput(Dir, 1.0f);
		}
	}

	// Периодически (RepathInterval) пробуем навмеш-путь домой с фильтром обхода деревни.
	if (Now - LastMoveIssueTime >= RepathInterval && GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		LastMoveIssueTime = Now;
		LastMoveResult = MoveToLocation(HomeLocation, ReturnAcceptRadius * 0.5f,
			/*bStopOnOverlap=*/true, /*bUsePathfinding=*/true,
			/*bProjectDestinationToNavigation=*/false, /*bCanStrafe=*/true,
			MoveFilterClass, /*bAllowPartialPath=*/true);
	}
}

void AEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APawn* Self = GetPawn();
	if (!Self)
	{
		return;
	}

	// Мёртвый враг (StatsComponent) ничего не делает.
	if (OwnStats && OwnStats->IsDead())
	{
		if (CurrentState != EEnemyAIState::Idle)
		{
			StopMovement();
			SetVillageSlowdown(false);
			CurrentState = EEnemyAIState::Idle;
		}
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// Ленивый кэш базовой скорости: к первому тику контроллера пешка уже применила свою
	// MaxWalkSpeed в BeginPlay (бандит 650 / волк ~780). Нужен для замедления у деревни.
	if (CachedBaseWalkSpeed <= 0.0f)
	{
		if (const ACharacter* SelfChar = Cast<ACharacter>(Self))
		{
			if (const UCharacterMovementComponent* Move = SelfChar->GetCharacterMovement())
			{
				CachedBaseWalkSpeed = Move->MaxWalkSpeed;
			}
		}
	}

	APawn* Player = GetPlayerPawn();

	// --- Состояния «отпустил игрока» (ADR-036) — до сенсинга: враг игнорирует игрока ---

	if (CurrentState == EEnemyAIState::Return)
	{
		TickReturnHome(Now);
		return;
	}

	if (CurrentState == EEnemyAIState::VillagePause)
	{
		// Стоит у границы деревни, смотрит на игрока, затем уходит домой.
		if (Player)
		{
			SetFocus(Player);
		}
		if (Now >= VillagePauseEndTime)
		{
			StartReturnHome(TEXT("village-pause-over"));
		}
		return;
	}

	const bool bSensed = CanSensePlayer(Player);

	if (!bSensed)
	{
		SetVillageSlowdown(false);

		// Игрок не обнаружен — Idle.
		if (CurrentState != EEnemyAIState::Idle)
		{
			StopMovement();
			ClearFocus(EAIFocusPriority::Gameplay);
			CurrentState = EEnemyAIState::Idle;
		}

		// Потерял игрока далеко от дома → идёт домой, а не стоит посреди карты (ADR-036).
		if (bLeashSet && ReturnHomeWhenIdleBeyond > 0.0f
			&& FVector::DistXY(Self->GetActorLocation(), HomeLocation) > ReturnHomeWhenIdleBeyond)
		{
			StartReturnHome(TEXT("idle-far-from-home"));
		}
		return;
	}

	// --- Поводок (ADR-036): слишком далеко от СВОЕЙ базы → разворот и возврат, не гонится ---
	if (bLeashSet && LeashRadius > 0.0f
		&& FVector::DistXY(Self->GetActorLocation(), HomeLocation) > LeashRadius)
	{
		StartReturnHome(TEXT("leash"));
		return;
	}

	// Краевой случай: враг сам оказался ВНУТРИ деревни (спавн/пуш) → немедленно уходит.
	if (bRespectVillageSafeZone && AVillageZone::IsPointInVillage(GetWorld(), Self->GetActorLocation()))
	{
		StartReturnHome(TEXT("inside-village"));
		return;
	}

	// Игрок обнаружен — смотрим на него.
	SetFocus(Player);

	// GetDistanceTo меряет ЦЕНТР-К-ЦЕНТРУ капсул. Дальность ножа AttackRange задана
	// поверхность-к-поверхности, поэтому переводим её в центр-к-центру, добавляя
	// сумму радиусов капсул. БАГ ДО ФИКСА: AttackRange(175) сравнивали напрямую с
	// center-distance, а MoveToActor (AcceptanceRadius 120 + reach-test добавляет радиусы
	// капсул) останавливал врага на center-distance ~120+радиусы (≈200+), что БОЛЬШЕ 175 →
	// враг тормозил дальше, чем мог достать ножом, и не атаковал, пока игрок сам не подойдёт.
	const float CombinedRadius = GetCombinedCapsuleRadius(Player);
	const float EffectiveAttackRange = AttackRange + CombinedRadius;
	const float Dist = Self->GetDistanceTo(Player);

	if (Dist <= EffectiveAttackRange)
	{
		// В радиусе ближней атаки. Бьём и из Standoff — игрок сам подошёл вплотную
		// (решение game-lead: standoff-враги защищаются при контакте).
		SetVillageSlowdown(false);
		if (CurrentState != EEnemyAIState::Attack)
		{
			StopMovement();
			CurrentState = EEnemyAIState::Attack;
		}
		PerformAttack(Player);
		return;
	}

	// --- Плотность боя (ADR-037): без свободного слота — держимся поодаль (Standoff) ---
	if (!HasAttackSlot(Player))
	{
		SetVillageSlowdown(false);
		if (CurrentState != EEnemyAIState::Standoff)
		{
			StopMovement();
			CurrentState = EEnemyAIState::Standoff;
			FQADebug::QA(GetWorld(), FString::Printf(
				TEXT("QA: %s standoff (attack slots full, max=%d)"),
				*Self->GetName(), MaxSimultaneousAttackers), /*bScreen=*/true);
		}
		return;
	}

	// --- Огнестрел (D6): в дальности и в кадре камеры игрока → стоит и стреляет (ADR-035).
	// Вне кадра НЕ стреляет (правило честности) — вместо этого сближается (ветка Chase ниже),
	// что естественно вводит его в кадр; HUD ведёт на него красную краевую стрелку.
	if (bRangedAttacker && Dist <= RangedAttackRange
		&& (!bHonorCameraFairness || IsSelfOnPlayerScreen()))
	{
		SetVillageSlowdown(false);
		if (CurrentState != EEnemyAIState::Attack)
		{
			StopMovement();
			CurrentState = EEnemyAIState::Attack;
		}
		PerformRangedAttack(Player);
		return;
	}

	// Далеко — преследуем.
	{
		if (CurrentState != EEnemyAIState::Chase)
		{
			CurrentState = EEnemyAIState::Chase;
			LastMoveIssueTime = -1000.0f; // на входе в Chase отдать move немедленно
			// На входе даём nav честный первый шанс (оптимистично RequestSuccessful), сбрасываем
			// трекинг сближения от текущей дистанции. Если первый MoveToActor реально вернёт Failed —
			// со следующего тика включится прямой ход.
			LastMoveResult = EPathFollowingRequestResult::RequestSuccessful;
			BestChaseDist = Dist;
			LastProgressTime = Now;
		}

		// --- Проекция позиций на навмеш (нужна и для решения о fallback, и для QA-лога) ---
		// Считаем КАЖДЫЙ тик (а не только в логе): по ней решаем, можно ли вообще навигировать.
		// Небольшой extent — чтобы ответ был честным «рядом ли навмеш», а не вытягивал далёкую точку.
		bool bSelfOnNav = false;
		bool bTargetOnNav = false;
		if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
		{
			const FVector QueryExtent(100.0f, 100.0f, 200.0f);
			FNavLocation Proj;
			bSelfOnNav = NavSys->ProjectPointToNavigation(Self->GetActorLocation(), Proj, QueryExtent);
			if (Player)
			{
				bTargetOnNav = NavSys->ProjectPointToNavigation(Player->GetActorLocation(), Proj, QueryExtent);
			}
		}

		// --- Трекинг сближения: реально ли убывает дистанция ---
		if (Dist < BestChaseDist - ChaseConvergeEpsilon)
		{
			BestChaseDist = Dist;
			LastProgressTime = Now;
		}
		const bool bNotConverging = (Now - LastProgressTime) >= StuckConvergeTime;

		// --- Решение: нормальная nav-погоня или прямой ход (fallback) ---
		// Direct включается, если путь по навмешу не строится / цель или враг вне навмеша /
		// враг застрял (не сближается). Иначе — обычная навигация (обходит препятствия в деревне).
		const bool bUseDirect =
			(LastMoveResult == EPathFollowingRequestResult::Failed) ||
			!bSelfOnNav || !bTargetOnNav || bNotConverging;

		// Запоминаем режим для headless QA-теста погони (IsChaseModeNavForQA).
		bLastChaseUsedDirect = bUseDirect;

		// --- Барьер деревни (ADR-036) В ОБОИХ режимах погони (nav И direct) ---
		// Направление фактического движения: в direct — прямо на игрока; в nav — текущая
		// скорость (путь может огибать), при стоянии — направление на игрока.
		FVector MoveDir = (Player->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
		if (!bUseDirect)
		{
			const FVector Velocity = Self->GetVelocity();
			if (Velocity.SizeSquared2D() > 2500.0f) // > 50 см/с — реально движется
			{
				MoveDir = Velocity.GetSafeNormal2D();
			}
		}

		if (bRespectVillageSafeZone && !MoveDir.IsNearlyZero())
		{
			if (IsAheadInVillage(MoveDir, VillageStopLookAhead))
			{
				// Граница деревни прямо по курсу: стоп, пауза «пару секунд», затем домой (ADR-036).
				StopMovement();
				SetVillageSlowdown(false);
				CurrentState = EEnemyAIState::VillagePause;
				VillagePauseEndTime = Now + VillagePauseSeconds;
				FQADebug::QA(GetWorld(), FString::Printf(
					TEXT("QA: %s stopped at village border (pause %.1fs)"),
					*Self->GetName(), VillagePauseSeconds), /*bScreen=*/true);
				return;
			}
			// Подходит к границе — замедляется (визуально «не хочет» заходить).
			SetVillageSlowdown(IsAheadInVillage(MoveDir, VillageSlowdownLookAhead));
		}
		else
		{
			SetVillageSlowdown(false);
		}

		if (bUseDirect)
		{
			// FALLBACK: прямой steering к игроку, игнорируя навмеш. AddMovementInput на пешке
			// (Character с UCharacterMovementComponent — бандит/волк) гарантированно сближает на
			// открытой местности. Гасим активный path-following, чтобы он не конфликтовал с ручным вводом.
			if (GetMoveStatus() == EPathFollowingStatus::Moving)
			{
				StopMovement();
			}

			const FVector ToPlayer = (Player->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
			if (!ToPlayer.IsNearlyZero())
			{
				Self->AddMovementInput(ToPlayer, 1.0f);
			}
		}
		else
		{
			// НОРМАЛЬНАЯ nav-погоня. Запускаем MoveToActor только когда path-following реально
			// завершился (Idle). Пока враг в Moving/Waiting — не прерываем: каждый перезапуск
			// отменял текущее движение и вызывал 1-2 кадра Waiting → рывки (дёрганье).
			// Движущийся игрок отслеживается автоматически: бандит достигает старой позиции
			// → Idle → следующий тик новый MoveToActor с актуальной позицией.
			const bool bNotMoving = (GetMoveStatus() == EPathFollowingStatus::Idle);
			if (bNotMoving)
			{
				LastMoveIssueTime = Now;
				// ЗАХВАТЫВАЕМ результат запроса move. Если путь к игроку не строится (пешка/цель вне
				// навмеша или навмеш не запечён) — здесь будет Failed → со следующего тика fallback.
				LastMoveResult = MoveToActor(Player, MoveAcceptanceRadius,
				/*bStopOnOverlap=*/true, /*bUsePathfinding=*/true, /*bCanStrafe=*/true,
				MoveFilterClass);
			}
		}

		// QA-диагностика погони (камера ненадёжна). Дросселируем по времени, чтобы не спамить.
		// СВЕДЁННАЯ строка: mode — режим (nav|direct), selfNav/targetNav — спроецированы ли враг/игрок
		// на навмеш, moveResult — итог последнего MoveToActor, dist — center-to-center дистанция.
		// Сборщик по mode видит, fallback ли это, по dist — убывает ли сближение.
		if (Now - LastChaseLogTime >= ChaseLogInterval)
		{
			LastChaseLogTime = Now;

			const TCHAR* MoveResultStr =
				(LastMoveResult == EPathFollowingRequestResult::Failed) ? TEXT("Failed") :
				(LastMoveResult == EPathFollowingRequestResult::AlreadyAtGoal) ? TEXT("AlreadyAtGoal") :
				TEXT("RequestSuccessful");

			FQADebug::QA(GetWorld(), FString::Printf(
				TEXT("QA: %s chase mode=%s dist=%.0f (selfNav=%s targetNav=%s moveResult=%s)"),
				*Self->GetName(),
				bUseDirect ? TEXT("direct") : TEXT("nav"),
				Dist,
				bSelfOnNav ? TEXT("yes") : TEXT("no"),
				bTargetOnNav ? TEXT("yes") : TEXT("no"),
				MoveResultStr), /*bScreen=*/true);
		}
	}
}
