// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyAIController.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyAIController::BeginPlay()
{
	Super::BeginPlay();
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	OwnStats = nullptr;
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(InPawn))
	{
		OwnStats = Enemy->GetStats();
	}

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

void AEnemyAIController::PerformAttack(APawn* Player)
{
	if (!Player)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastAttackTime < AttackCooldown)
	{
		return; // ещё на кулдауне
	}

	LastAttackTime = Now;

	// Урон игроку через стандартный пайплайн UE
	// (у игрока — инлайн-Health базы AMasterHumanoidCharacter::TakeDamage).
	FDamageEvent DamageEvent;
	Player->TakeDamage(AttackDamage, DamageEvent, this, GetPawn());

	UE_LOG(LogTemp, Log, TEXT("%s attacks player for %.1f"),
		GetPawn() ? *GetPawn()->GetName() : TEXT("Enemy"), AttackDamage);
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

	const float Dist = Self->GetDistanceTo(Player);

	if (Dist <= AttackRange)
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
		}
		MoveToActor(Player, MoveAcceptanceRadius);
	}
}
