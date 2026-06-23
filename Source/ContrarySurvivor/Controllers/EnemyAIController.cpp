// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyAIController.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "Navigation/PathFollowingComponent.h" // EPathFollowingStatus (GetMoveStatus в Chase), EPathFollowingRequestResult
#include "NavigationSystem.h" // UNavigationSystemV1::GetCurrent / ProjectPointToNavigation + FNavLocation (QA-диагностика навмеша)
#include "ContrarySurvivor/Debug/QADebug.h" // QA-лог погони (дросселированный)
#include "ContrarySurvivor/Navigation/NavQueryFilter_ExcludeVillage.h" // BugReport 12: обход деревни

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

	// Урон игроку через стандартный пайплайн UE.
	FDamageEvent DamageEvent;
	Player->TakeDamage(AttackDamage, DamageEvent, this, GetPawn());

	UE_LOG(LogTemp, Log, TEXT("%s attacks player for %.1f"),
		GetPawn() ? *GetPawn()->GetName() : TEXT("Enemy"), AttackDamage);

	return true;
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
			CurrentState = EEnemyAIState::Idle;
		}
		return;
	}

	APawn* Player = GetPlayerPawn();
	const bool bSensed = CanSensePlayer(Player);

	if (!bSensed)
	{
		// Игрок не обнаружен — Idle.
		if (CurrentState != EEnemyAIState::Idle)
		{
			StopMovement();
			ClearFocus(EAIFocusPriority::Gameplay);
			CurrentState = EEnemyAIState::Idle;
		}
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
		// В радиусе атаки.
		if (CurrentState != EEnemyAIState::Attack)
		{
			StopMovement();
			CurrentState = EEnemyAIState::Attack;
		}
		PerformAttack(Player);
	}
	else
	{
		// Далеко — преследуем.
		const float Now = GetWorld()->GetTimeSeconds();

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

			// ПРОБА nav на восстановление: ВРЕМЕННО ОТКЛЮЧЕНА (тест дёргания).
			// Гипотеза: MoveToActor каждые 0.35с вызывает AbortMove внутри → CMC velocity сбрасывается
			// → "шаг-стоп-шаг-стоп". При selfNav=no зонд всегда Failed и только мешает.
			// Если дёрганье исчезнет — убрать насовсем; если нет — искать дальше.
			// if (Now - LastMoveIssueTime >= RepathInterval)
			// {
			// 	LastMoveIssueTime = Now;
			// 	LastMoveResult = MoveToActor(Player, MoveAcceptanceRadius,
			// 	/*bStopOnOverlap=*/true, /*bUsePathfinding=*/true, /*bCanStrafe=*/true,
			// 	MoveFilterClass);
			// }
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
