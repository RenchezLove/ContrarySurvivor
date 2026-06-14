// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfSpawnSubsystem.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"
#include "ContrarySurvivor/Debug/QADebug.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "SpawnPlacementUtils.h"

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

	// Деревня — мирная зона: НЕ спавним волков у игрока на старте. Вместо этого ждём
	// приближения игрока к Логову — повторяющийся таймер проверки дистанции.
	InWorld.GetTimerManager().SetTimer(
		ActivationTimerHandle, this, &UWolfSpawnSubsystem::CheckActivation,
		ActivationCheckPeriod, /*bLoop=*/true, /*FirstDelay=*/ActivationCheckPeriod);

	FQADebug::QA(&InWorld,
		FString::Printf(TEXT("QA: WolfDen armed at %s, R=%.0f (peaceful village, wolves wait at den)"),
			*WolfDenLocation.ToString(), ActivationRadius),
		true);
}

void UWolfSpawnSubsystem::CheckActivation()
{
	if (bWolvesSpawned)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!PlayerPawn)
	{
		return; // игрок ещё не заспавнен — ждём следующего тика таймера
	}

	// Горизонтальная (XY) дистанция игрока до центра Логова: Z игнорируем (рельеф/высота
	// игрока не должны влиять на триггер активации).
	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	const float DistSqXY = FVector::DistSquaredXY(PlayerLoc, WolfDenLocation);
	const float RadiusSq = ActivationRadius * ActivationRadius;

	if (DistSqXY <= RadiusSq)
	{
		SpawnWolvesAtDen();
		bWolvesSpawned = true; // одноразово

		// Активация отработала — гасим таймер проверки.
		World->GetTimerManager().ClearTimer(ActivationTimerHandle);
	}
}

bool UWolfSpawnSubsystem::QAForceSpawnWolves()
{
	if (bWolvesSpawned)
	{
		return false; // уже заспавнены (идемпотентно)
	}

	if (!WolfClass)
	{
		WolfClass = AWolfCharacter::StaticClass();
	}

	SpawnWolvesAtDen();
	bWolvesSpawned = true; // одноразово (как штатная активация)

	// Гасим таймер проверки дистанции — активация отработана принудительно.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActivationTimerHandle);
	}

	FQADebug::QA(GetWorld(), TEXT("QA-TEST: WolfDen force-activated (cs.TestWolfChase)"), /*bScreen=*/true);
	return true;
}

void UWolfSpawnSubsystem::SpawnWolvesAtDen()
{
	UWorld* World = GetWorld();
	if (!World || !WolfClass)
	{
		return;
	}

	const int32 Count = FMath::Max(1, NumWolves);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	int32 Spawned = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		// Раскладываем волков равномерно по кругу вокруг центра Логова.
		const float AngleDeg = (360.0f / Count) * i;
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);
		FVector DesiredLoc = WolfDenLocation + FVector(
			FMath::Cos(AngleRad) * DenSpreadRadius,
			FMath::Sin(AngleRad) * DenSpreadRadius,
			0.0f);

		// Проецируем XY на навмеш, чтобы волк встал в проходимой точке (не в стволе/камне).
		FVector ProjectedLoc = DesiredLoc;
		FVector ProjectedOut;
		const bool bProjected = UNavigationSystemV1::K2_ProjectPointToNavigation(
			World, DesiredLoc, ProjectedOut, /*NavData=*/nullptr, /*FilterClass=*/nullptr,
			FVector(600.0f, 600.0f, 600.0f));
		if (bProjected)
		{
			// Высоту НЕ берём от навмеш-проекции/игрока: трасса до пола в этой XY (надёжный Z).
			ProjectedLoc.X = ProjectedOut.X;
			ProjectedLoc.Y = ProjectedOut.Y;
		}
		ProjectedLoc.Z = SpawnPlacement::ResolveSpawnZ(
			World, ProjectedLoc.X, ProjectedLoc.Y, /*ZOffset=*/90.0f, TEXT("Wolf"));

		// Разворачиваем волка лицом к центру Логова.
		const FRotator SpawnRot = (WolfDenLocation - ProjectedLoc).Rotation();
		AWolfCharacter* Wolf = World->SpawnActor<AWolfCharacter>(
			WolfClass, ProjectedLoc, FRotator(0.0f, SpawnRot.Yaw, 0.0f), SpawnParams);

		if (Wolf)
		{
			++Spawned;
			UE_LOG(LogTemp, Log, TEXT("WolfSpawnSubsystem: spawned %s at den %s (navmesh=%s)"),
				*Wolf->GetName(), *ProjectedLoc.ToString(), bProjected ? TEXT("yes") : TEXT("floor-trace"));
		}
	}

	FQADebug::QA(World,
		FString::Printf(TEXT("QA: WolfDen activated, spawned %d wolves at den"), Spawned),
		true);
}
