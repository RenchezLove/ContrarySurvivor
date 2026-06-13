// Fill out your copyright notice in the Description page of Project Settings.

#include "TraderSpawnSubsystem.h"
#include "ContrarySurvivor/Actors/TraderNPC.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

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
	// Ставим торговца перед игроком (по +X), дистанция SpawnDistance.
	FVector DesiredLoc = PlayerLoc + FVector(SpawnDistance, 0.0f, 0.0f);

	// Проецируем на навмеш, чтобы торговец стоял в проходимой точке.
	FVector SpawnLoc = DesiredLoc;
	FVector ProjectedOut;
	const bool bProjected = UNavigationSystemV1::K2_ProjectPointToNavigation(
		World, DesiredLoc, ProjectedOut, /*NavData=*/nullptr, /*FilterClass=*/nullptr,
		FVector(800.0f, 800.0f, 800.0f));
	if (bProjected)
	{
		SpawnLoc = ProjectedOut + FVector(0.0f, 0.0f, 90.0f);
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ATraderNPC* Trader = World->SpawnActor<ATraderNPC>(TraderClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
	if (Trader)
	{
		UE_LOG(LogTemp, Log, TEXT("TraderSpawnSubsystem: spawned %s at %s (navmesh=%s)"),
			*Trader->GetName(), *SpawnLoc.ToString(), bProjected ? TEXT("yes") : TEXT("fallback"));
	}
}
