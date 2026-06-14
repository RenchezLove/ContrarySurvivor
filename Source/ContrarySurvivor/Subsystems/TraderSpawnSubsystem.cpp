// Fill out your copyright notice in the Description page of Project Settings.

#include "TraderSpawnSubsystem.h"
#include "ContrarySurvivor/Actors/TraderNPC.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "SpawnPlacementUtils.h"

void UTraderSpawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld())
	{
		return;
	}

	if (!TraderClass)
	{
		TraderClass = ATraderNPC::StaticClass();
	}

	InWorld.GetTimerManager().SetTimer(
		SpawnTimerHandle, this, &UTraderSpawnSubsystem::SpawnTrader, SpawnDelay, false);
}

void UTraderSpawnSubsystem::SpawnTrader()
{
	UWorld* World = GetWorld();
	if (!World || !TraderClass)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("TraderSpawnSubsystem: no player pawn, skip trader spawn"));
		return;
	}

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();

	// BUG4: раньше торговец ставился жёстко по +X от игрока и при неудачной проекции мог
	// оказаться в стене/доме/за деревней. Теперь пробуем КОЛЬЦО направлений вокруг игрока и
	// берём первую точку, спроецированную на навмеш. Игрок стоит в деревне на навмеше, поэтому
	// соседняя проходимая точка — тоже деревня (рядом с домами, не под землёй/не за картой).
	static const float CandidateAnglesDeg[] = { 0.0f, 45.0f, 90.0f, 135.0f, 180.0f, 225.0f, 270.0f, 315.0f };

	FVector SpawnLoc = PlayerLoc + FVector(SpawnDistance, 0.0f, 0.0f); // фолбэк по умолчанию
	bool bProjected = false;

	for (float Deg : CandidateAnglesDeg)
	{
		const float Rad = FMath::DegreesToRadians(Deg);
		const FVector DesiredLoc = PlayerLoc + FVector(FMath::Cos(Rad) * SpawnDistance,
		                                               FMath::Sin(Rad) * SpawnDistance,
		                                               0.0f);
		FVector ProjectedOut;
		if (UNavigationSystemV1::K2_ProjectPointToNavigation(
			World, DesiredLoc, ProjectedOut, /*NavData=*/nullptr, /*FilterClass=*/nullptr,
			FVector(400.0f, 400.0f, 600.0f)))
		{
			SpawnLoc = ProjectedOut + FVector(0.0f, 0.0f, 90.0f);
			bProjected = true;
			break;
		}
	}

	// Если ни одно направление не спроецировалось — проецируем саму точку игрока (он на навмеше).
	if (!bProjected)
	{
		FVector ProjectedOut;
		if (UNavigationSystemV1::K2_ProjectPointToNavigation(
			World, PlayerLoc, ProjectedOut, /*NavData=*/nullptr, /*FilterClass=*/nullptr,
			FVector(200.0f, 200.0f, 600.0f)))
		{
			SpawnLoc = ProjectedOut + FVector(SpawnDistance * 0.5f, 0.0f, 90.0f);
			bProjected = true;
		}
		else
		{
			// Навмеш недоступен (например, ещё не достроен в рантайме). Раньше брали "сырой"
			// fallback с Z игрока — если игрок под картой (Z=-4055), торговец тоже уходил под пол.
			// Теперь ставим высоту НЕ от навмеша, а трассировкой до пола в этой XY-точке.
			UE_LOG(LogTemp, Warning,
				TEXT("TraderSpawnSubsystem: navmesh projection FAILED in all directions; using floor-trace fallback. Проверь NavMeshBoundsVolume в деревне."));
			SpawnLoc.Z = SpawnPlacement::ResolveSpawnZ(World, SpawnLoc.X, SpawnLoc.Y, /*ZOffset=*/90.0f, TEXT("Trader"));
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATraderNPC* Trader = World->SpawnActor<ATraderNPC>(TraderClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
	if (Trader)
	{
		UE_LOG(LogTemp, Log, TEXT("TraderSpawnSubsystem: spawned %s at %s (playerLoc %s, navmesh=%s)"),
			*Trader->GetName(), *SpawnLoc.ToString(), *PlayerLoc.ToString(), bProjected ? TEXT("yes") : TEXT("fallback"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TraderSpawnSubsystem: FAILED to spawn trader at %s"), *SpawnLoc.ToString());
	}
}
