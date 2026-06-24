// Fill out your copyright notice in the Description page of Project Settings.

#include "BanditSpawnSubsystem.h"
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Actors/Pickup.h"   // пикап-носитель ноутбука
#include "ContrarySurvivor/Debug/QADebug.h"
#include "AQuestItem.h"                         // класс квест-предмета «Ноутбук»
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"
#include "SpawnPlacementUtils.h"
#include "UObject/ConstructorHelpers.h"

UBanditSpawnSubsystem::UBanditSpawnSubsystem()
{
	// BP_EnemyBandit несёт модульный визуал (меши Head/Torso/Legs назначены в редакторе) и AI.
	// FClassFinder без дота сам добавляет ".<name>_C".
	static ConstructorHelpers::FClassFinder<AEnemyCharacter> BanditBP(
		TEXT("/Game/Characters/Bandit/BP_EnemyBandit"));
	if (BanditBP.Succeeded())
	{
		BanditClass = BanditBP.Class;
	}

	// Квест-предмет «Ноутбук» и пикап-носитель — голые C++-классы (editor-независимо).
	QuestItemClass = AQuestItem::StaticClass();
	PickupClass = APickup::StaticClass();
}

void UBanditSpawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Только игровые миры (PIE/Standalone) — не превью/редактор.
	if (!InWorld.IsGameWorld())
	{
		return;
	}

	// Fallback на голый C++ AEnemyCharacter, если BP_EnemyBandit не найден (визуал будет хуже,
	// но логика спавна/боя сохранится).
	if (!BanditClass)
	{
		BanditClass = AEnemyCharacter::StaticClass();
	}

	// База бандитов ждёт приближения игрока — повторяющийся таймер проверки дистанции.
	InWorld.GetTimerManager().SetTimer(
		ActivationTimerHandle, this, &UBanditSpawnSubsystem::CheckActivation,
		ActivationCheckPeriod, /*bLoop=*/true, /*FirstDelay=*/ActivationCheckPeriod);

	FQADebug::QA(&InWorld,
		FString::Printf(TEXT("QA: BanditBase armed at %s, R=%.0f (peaceful village, bandits wait at base)"),
			*BanditBaseLocation.ToString(), ActivationRadius),
		true);
}

void UBanditSpawnSubsystem::CheckActivation()
{
	if (bBanditsSpawned)
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

	// Горизонтальная (XY) дистанция игрока до центра базы: Z игнорируем (рельеф/высота игрока
	// не должны влиять на триггер активации).
	const FVector PlayerLoc = PlayerPawn->GetActorLocation();
	const float DistSqXY = FVector::DistSquaredXY(PlayerLoc, BanditBaseLocation);
	const float RadiusSq = ActivationRadius * ActivationRadius;

	if (DistSqXY <= RadiusSq)
	{
		bBanditsSpawned = true; // одноразово, сразу — чтобы CheckActivation не перезапланировал спавн
		World->GetTimerManager().ClearTimer(ActivationTimerHandle); // активация отработала

		// НЕ спавним мгновенно: игрок только что прибыл к базе (телепорт Z), Navigation Invoker на
		// игроке ещё не успел построить навмеш-тайлы вокруг этой точки в ТОТ ЖЕ кадр → бандиты сели
		// бы navmesh=floor-trace и не навигировали. Даём инвокеру SpawnDelay секунд на построение
		// тайлов, затем спавним — бандиты сядут navmesh=yes.
		World->GetTimerManager().SetTimer(
			SpawnDelayTimerHandle, this, &UBanditSpawnSubsystem::DoDelayedSpawn,
			SpawnDelay, /*bLoop=*/false);

		FQADebug::QA(World, FString::Printf(
			TEXT("QA: BanditBase player in range, spawning in %.1fs (waiting for navmesh build)"), SpawnDelay),
			true);
	}
}

void UBanditSpawnSubsystem::DoDelayedSpawn()
{
	// Отложенный спавн (после паузы на построение навмеша инвокером).
	SpawnBanditsAtBase();
	SpawnQuestNotebookAtBase(); // Фаза 5: ноутбук (кв.2) появляется на базе вместе с бандитами
}

void UBanditSpawnSubsystem::SpawnBanditsAtBase()
{
	UWorld* World = GetWorld();
	if (!World || !BanditClass)
	{
		return;
	}

	const int32 Count = FMath::Max(1, NumBandits);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	int32 Spawned = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		// Раскладываем бандитов равномерно по кругу вокруг центра базы.
		const float AngleDeg = (360.0f / Count) * i;
		const float AngleRad = FMath::DegreesToRadians(AngleDeg);
		FVector DesiredLoc = BanditBaseLocation + FVector(
			FMath::Cos(AngleRad) * SpreadRadius,
			FMath::Sin(AngleRad) * SpreadRadius,
			0.0f);

		// Проецируем XY на навмеш, чтобы бандит встал в проходимой точке (не в стене/сарае).
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
			World, ProjectedLoc.X, ProjectedLoc.Y, /*ZOffset=*/90.0f, TEXT("Bandit"));

		// Разворачиваем бандита лицом к центру базы.
		const FRotator SpawnRot = (BanditBaseLocation - ProjectedLoc).Rotation();
		AEnemyCharacter* Bandit = World->SpawnActor<AEnemyCharacter>(
			BanditClass, ProjectedLoc, FRotator(0.0f, SpawnRot.Yaw, 0.0f), SpawnParams);

		if (Bandit)
		{
			++Spawned;
			UE_LOG(LogTemp, Log, TEXT("BanditSpawnSubsystem: spawned %s at base %s (navmesh=%s)"),
				*Bandit->GetName(), *ProjectedLoc.ToString(), bProjected ? TEXT("yes") : TEXT("floor-trace"));
		}
	}

	FQADebug::QA(World,
		FString::Printf(TEXT("QA: BanditBase activated, spawned %d bandits"), Spawned),
		true);
}

void UBanditSpawnSubsystem::SpawnQuestNotebookAtBase()
{
	UWorld* World = GetWorld();
	if (!World || !QuestItemClass)
	{
		return;
	}

	// Ноутбук кладём в центр базы. XY проецируем на навмеш (чтобы лежал в проходимой точке),
	// высоту — трассой до пола (как у бандитов). Кладём чуть выше пола (+20), как «лут на земле».
	FVector Loc = BanditBaseLocation;
	FVector ProjectedOut;
	const bool bProjected = UNavigationSystemV1::K2_ProjectPointToNavigation(
		World, BanditBaseLocation, ProjectedOut, /*NavData=*/nullptr, /*FilterClass=*/nullptr,
		FVector(600.0f, 600.0f, 600.0f));
	if (bProjected)
	{
		Loc.X = ProjectedOut.X;
		Loc.Y = ProjectedOut.Y;
	}
	Loc.Z = SpawnPlacement::ResolveSpawnZ(World, Loc.X, Loc.Y, /*ZOffset=*/20.0f, TEXT("Notebook"));

	// Пикап-носитель с гарантированным предметом (chance=1.0), без денег. Имя предмета = QuestItemName
	// («Ноутбук»), совпадает с RequiredItemName кв.2 у старосты. Подбор — по E (как любой пикап).
	APickup* Pickup = APickup::DropLoot(World, Loc, /*Money=*/0.0f,
		QuestItemClass, /*ItemDropChance=*/1.0f, PickupClass, QuestItemName);

	FQADebug::QA(World, Pickup
		? FString::Printf(TEXT("QA: quest notebook '%s' spawned at base %s"), *QuestItemName, *Loc.ToCompactString())
		: TEXT("QA: quest notebook spawn FAILED"), /*bScreen=*/true);
}
