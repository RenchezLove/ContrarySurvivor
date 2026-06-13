// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfSpawnSubsystem.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

void UWolfSpawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Только игровые миры (PIE/Standalone) — не превью/редактор.
	if (!InWorld.IsGameWorld())
	{
		return;
	}

	if (!WolfClass)
	{
		WolfClass = AWolfCharacter::StaticClass();
	}

	// Спавним с задержкой: к OnWorldBeginPlay игрок может быть ещё не создан.
	InWorld.GetTimerManager().SetTimer(
		SpawnTimerHandle, this, &UWolfSpawnSubsystem::SpawnWolves, SpawnDelay, false);
}

void UWolfSpawnSubsystem::SpawnWolves()
{
	UWorld* World = GetWorld();
	if (!World || !WolfClass)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("WolfSpawnSubsystem: no player pawn, skip wolf spawn"));
		return;
	}

	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	const int32 Count = FMath::Max(1, NumWolves);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	int32 Spawned = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		// Раскладываем волков по дуге вокруг игрока на дистанции SpawnDistance.
		const float AngleDeg = 40.0f + (i * 80.0f); // 40, 120, ... — спереди-сбоку от игрока
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);
		const FVector Offset(FMath::Cos(AngleRad) * SpawnDistance,
		                     FMath::Sin(AngleRad) * SpawnDistance,
		                     0.0f);
		FVector DesiredLoc = PlayerLoc + Offset;

		// Проецируем на навмеш, чтобы волк встал в проходимой точке (не в доме/стене).
		FVector ProjectedLoc = DesiredLoc;
		FVector ProjectedOut;
		const bool bProjected = UNavigationSystemV1::K2_ProjectPointToNavigation(
			World, DesiredLoc, ProjectedOut, /*NavData=*/nullptr, /*FilterClass=*/nullptr,
			FVector(600.0f, 600.0f, 600.0f));
		if (bProjected)
		{
			// Чуть приподнимаем над навмешем, чтобы капсула не утонула.
			ProjectedLoc = ProjectedOut + FVector(0.0f, 0.0f, 90.0f);
		}

		const FRotator SpawnRot = (PlayerLoc - ProjectedLoc).Rotation();
		AWolfCharacter* Wolf = World->SpawnActor<AWolfCharacter>(
			WolfClass, ProjectedLoc, FRotator(0.0f, SpawnRot.Yaw, 0.0f), SpawnParams);

		if (Wolf)
		{
			++Spawned;
			UE_LOG(LogTemp, Log, TEXT("WolfSpawnSubsystem: spawned %s at %s (navmesh=%s)"),
				*Wolf->GetName(), *ProjectedLoc.ToString(), bProjected ? TEXT("yes") : TEXT("fallback"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("WolfSpawnSubsystem: spawned %d/%d wolves near player"), Spawned, Count);
}
