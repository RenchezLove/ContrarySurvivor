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

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;

	// До первого реального MoveToActor считаем запрос непроведённым (Failed).
	LastMoveResult = EPathFollowingRequestResult::Failed;
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
		if (CurrentState != EEnemyAIState::Chase)
		{
			CurrentState = EEnemyAIState::Chase;
			LastMoveIssueTime = -1000.0f; // на входе в Chase отдать move немедленно
		}

		// ПЕРЕОТДАЁМ MoveToActor НЕПРЕРЫВНО, пока в Chase и игрок в радиусе.
		// БАГ ДО ФИКСА: MoveToActor вызывался КАЖДЫЙ кадр — каждый вызов рестартил path-request,
		// пешка не успевала продвинуться и фактически стояла (погоня логнулась один раз и встала).
		// ФИКС: отдаём move заново только когда path-following НЕ в состоянии Moving
		// (завершился/зафейлился/Idle — иначе пешка встала бы навсегда) ЛИБО периодически раз в
		// RepathInterval (чтобы цель отслеживала движущегося игрока). Так враг реально сближается.
		const float NowMove = GetWorld()->GetTimeSeconds();
		const bool bNotMoving = (GetMoveStatus() != EPathFollowingStatus::Moving);
		if (bNotMoving || (NowMove - LastMoveIssueTime >= RepathInterval))
		{
			LastMoveIssueTime = NowMove;
			// ЗАХВАТЫВАЕМ результат запроса move (раньше игнорировался). Если путь к игроку не
			// строится (пешка/цель вне навмеша или навмеш не запечён) — здесь будет Failed.
			LastMoveResult = MoveToActor(Player, MoveAcceptanceRadius);
		}

		// QA-диагностика погони (камера ненадёжна). Дросселируем по времени, чтобы не спамить
		// каждый тик: пишем не чаще раза в ChaseLogInterval сек.
		// СВЕДЁННАЯ строка даёт QA различить причину провала погони:
		//   selfNav  — спроецирована ли позиция ВРАГА на навмеш (нет → враг стоит вне навмеша);
		//   targetNav— спроецирована ли позиция ИГРОКА на навмеш (нет → цель недостижима);
		//   moveResult — итог последнего MoveToActor (Failed/AlreadyAtGoal/RequestSuccessful);
		//   dist     — center-to-center дистанция до игрока.
		const float NowChase = GetWorld()->GetTimeSeconds();
		if (NowChase - LastChaseLogTime >= ChaseLogInterval)
		{
			LastChaseLogTime = NowChase;

			// Проекция позиций на навмеш. Небольшой extent (по умолчанию nav-системы мог бы
			// «вытянуть» далёкую точку — берём умеренный, чтобы ответ был честным «рядом ли навмеш»).
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

			const TCHAR* MoveResultStr =
				(LastMoveResult == EPathFollowingRequestResult::Failed) ? TEXT("Failed") :
				(LastMoveResult == EPathFollowingRequestResult::AlreadyAtGoal) ? TEXT("AlreadyAtGoal") :
				TEXT("RequestSuccessful");

			FQADebug::QA(GetWorld(), FString::Printf(
				TEXT("QA: %s chase: selfNav=%s targetNav=%s moveResult=%s dist=%.0f"),
				*Self->GetName(),
				bSelfOnNav ? TEXT("yes") : TEXT("no"),
				bTargetOnNav ? TEXT("yes") : TEXT("no"),
				MoveResultStr,
				Dist), /*bScreen=*/true);
		}
	}
}
