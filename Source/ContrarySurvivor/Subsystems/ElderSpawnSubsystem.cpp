// Fill out your copyright notice in the Description page of Project Settings.

#include "ElderSpawnSubsystem.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "SpawnPlacementUtils.h"

void UElderSpawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (!InWorld.IsGameWorld())
	{
		return;
	}

	if (!ElderClass)
	{
		ElderClass = AElderNPC::StaticClass();
	}

	InWorld.GetTimerManager().SetTimer(
		SpawnTimerHandle, this, &UElderSpawnSubsystem::SpawnElder, SpawnDelay, false);
}

void UElderSpawnSubsystem::SpawnElder()
{
	UWorld* World = GetWorld();
	if (!World || !ElderClass)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("ElderSpawnSubsystem: no player pawn, skip elder spawn"));
		return;
	}

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();

	// Перебор направлений с ПРОТИВОПОЛОЖНОЙ стороны (180° первым), чтобы не встать под торговцем
	// (он начинает с 0°). Берём первую точку, спроецированную на навмеш (деревня проходима).
	static const float CandidateAnglesDeg[] = { 180.0f, 225.0f, 135.0f, 270.0f, 90.0f, 315.0f, 45.0f, 0.0f };

	FVector SpawnLoc = PlayerLoc + FVector(-SpawnDistance, 0.0f, 0.0f); // фолбэк (противоположно торговцу)
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

	if (!bProjected)
	{
		FVector ProjectedOut;
		if (UNavigationSystemV1::K2_ProjectPointToNavigation(
			World, PlayerLoc, ProjectedOut, /*NavData=*/nullptr, /*FilterClass=*/nullptr,
			FVector(200.0f, 200.0f, 600.0f)))
		{
			SpawnLoc = ProjectedOut + FVector(-SpawnDistance * 0.5f, 0.0f, 90.0f);
			bProjected = true;
		}
		else
		{
			// Навмеш недоступен — высоту берём трассировкой до пола (не от Z игрока, который
			// мог провалиться под карту). См. SpawnPlacementUtils / BUG спавна на Z=-4055.
			UE_LOG(LogTemp, Warning,
				TEXT("ElderSpawnSubsystem: navmesh projection FAILED; using floor-trace fallback. Проверь NavMeshBoundsVolume."));
			SpawnLoc.Z = SpawnPlacement::ResolveSpawnZ(World, SpawnLoc.X, SpawnLoc.Y, /*ZOffset=*/90.0f, TEXT("Elder"));
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AElderNPC* Elder = World->SpawnActor<AElderNPC>(ElderClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
	if (Elder)
	{
		UE_LOG(LogTemp, Log, TEXT("ElderSpawnSubsystem: spawned %s at %s (playerLoc %s, navmesh=%s)"),
			*Elder->GetName(), *SpawnLoc.ToString(), *PlayerLoc.ToString(), bProjected ? TEXT("yes") : TEXT("fallback"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ElderSpawnSubsystem: FAILED to spawn elder at %s"), *SpawnLoc.ToString());
	}
}
