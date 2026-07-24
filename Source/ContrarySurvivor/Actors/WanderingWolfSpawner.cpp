// Fill out your copyright notice in the Description page of Project Settings.

#include "WanderingWolfSpawner.h"
#include "Campfire.h"           // «зелёная зона» костра: пересечение игрока с триггером
#include "MasterEnemyBase.h"    // базы врагов (логово волков / база бандитов) — радиус исключения
#include "VillageZone.h"        // деревня = безопасная зона (ADR-036), статический запрос
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h" // IsDead — мёртвые выбывают из лимита
#include "ContrarySurvivor/Subsystems/SpawnPlacementUtils.h" // ResolveSpawnZ (трасса до пола)
#include "Engine/World.h"
#include "EngineUtils.h" // TActorIterator
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

AWanderingWolfSpawner::AWanderingWolfSpawner()
{
	// Тик не нужен — вся логика на повторяющемся таймере SpawnInterval.
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	EditorMarker = CreateDefaultSubobject<USphereComponent>(TEXT("EditorMarker"));
	EditorMarker->SetupAttachment(SceneRoot);
	EditorMarker->InitSphereRadius(100.0f);
	EditorMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EditorMarker->SetCanEverAffectNavigation(false);
	EditorMarker->ShapeColor = FColor(120, 200, 120, 255); // зелёный — «спавнер живности»
}

void AWanderingWolfSpawner::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		SpawnTimerHandle, this, &AWanderingWolfSpawner::TrySpawnWolf,
		FMath::Max(1.0f, SpawnInterval), /*bLoop=*/true, /*FirstDelay=*/FMath::Max(1.0f, SpawnInterval));

	UE_LOG(LogTemp, Log, TEXT("WanderingWolfSpawner '%s': armed (interval=%.0fs, dist=%.0f..%.0f, max alive=%d, enabled=%d)"),
		*GetName(), SpawnInterval, MinSpawnDistance, MaxSpawnDistance, MaxAliveWolves, bEnabled ? 1 : 0);
}

void AWanderingWolfSpawner::TrySpawnWolf()
{
	if (!bEnabled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!WolfClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("WanderingWolfSpawner '%s': WolfClass not set (assign BP_Wolf in BP wrapper) — spawn skipped"),
			*GetName());
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player)
	{
		return; // игрок ещё не заспавнен — ждём следующего тика
	}

	PruneDeadWolves();
	if (AliveWolves.Num() >= MaxAliveWolves)
	{
		return; // лимит живых бродячих волков исчерпан
	}

	// Условия Рината: «если игрок не в зеленой зоне и не возле одной из баз противника».
	if (IsPlayerInSafeArea(*Player) || IsPlayerNearEnemyBase(*Player))
	{
		return;
	}

	const FVector PlayerLoc = Player->GetActorLocation();
	const float MaxDist = FMath::Max(MinSpawnDistance, MaxSpawnDistance);

	for (int32 Attempt = 0; Attempt < FMath::Max(1, MaxSpawnAttempts); ++Attempt)
	{
		const float AngleRad = FMath::FRandRange(0.0f, 2.0f * PI);
		const float Dist = FMath::FRandRange(MinSpawnDistance, MaxDist);
		FVector Candidate = PlayerLoc + FVector(FMath::Cos(AngleRad) * Dist, FMath::Sin(AngleRad) * Dist, 0.0f);

		// Обязательная навмеш-проекция: не село на навмеш — точка невалидна, тихо пропускаем
		// попытку (требование задачи). Экстент 500 « MinSpawnDistance — далеко точку не утащит.
		FVector ProjectedOut;
		const bool bProjected = UNavigationSystemV1::K2_ProjectPointToNavigation(
			World, Candidate, ProjectedOut, /*NavData=*/nullptr, /*FilterClass=*/nullptr,
			FVector(500.0f, 500.0f, 500.0f));
		if (!bProjected)
		{
			continue;
		}
		Candidate.X = ProjectedOut.X;
		Candidate.Y = ProjectedOut.Y;

		// Высоту навмешу не доверяем — трасса до пола, как у AMasterEnemyBase.
		Candidate.Z = SpawnPlacement::ResolveSpawnZ(
			World, Candidate.X, Candidate.Y, /*ZOffset=*/90.0f, TEXT("WanderingWolf"));

		if (!IsSpawnPointClearOfZones(Candidate))
		{
			continue;
		}

		if (IsPointOnPlayerScreen(Candidate))
		{
			continue; // точка в кадре камеры — волк появился бы на глазах
		}

		// Точка годна — спавним волка лицом в сторону игрока.
		const float Yaw = (PlayerLoc - Candidate).Rotation().Yaw;
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ACharacter* Wolf = World->SpawnActor<ACharacter>(WolfClass, Candidate, FRotator(0.0f, Yaw, 0.0f), SpawnParams);
		if (Wolf)
		{
			AliveWolves.Add(Wolf);
			UE_LOG(LogTemp, Log, TEXT("WanderingWolfSpawner '%s': spawned %s at %s (dist to player %.0f, alive %d/%d, attempt %d)"),
				*GetName(), *Wolf->GetName(), *Candidate.ToCompactString(),
				FVector::DistXY(Candidate, PlayerLoc), AliveWolves.Num(), MaxAliveWolves, Attempt + 1);
		}
		return; // один волк (одна попытка SpawnActor) за тик таймера
	}

	// Все попытки мимо (навмеш/зоны/кадр камеры) — молча ждём следующего тика (по задаче).
	UE_LOG(LogTemp, Verbose, TEXT("WanderingWolfSpawner '%s': no valid spawn point this tick (%d attempts)"),
		*GetName(), MaxSpawnAttempts);
}

void AWanderingWolfSpawner::PruneDeadWolves()
{
	AliveWolves.RemoveAll([](const TWeakObjectPtr<ACharacter>& WolfPtr)
	{
		const ACharacter* Wolf = WolfPtr.Get();
		if (!Wolf)
		{
			return true; // уничтожен (труп убран по CorpseLifeSpan / смена уровня)
		}
		// Мёртвый, но труп ещё лежит — из лимита тоже выбывает (лимит — по ЖИВЫМ).
		const UStatsComponent* Stats = Wolf->FindComponentByClass<UStatsComponent>();
		return Stats && Stats->IsDead();
	});
}

bool AWanderingWolfSpawner::IsPlayerInSafeArea(const APawn& Player) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// «Зелёная зона» костра: игрок пересекается с триггером любого костра. Радиус зоны хранит
	// сам костёр (SafeZoneTrigger) — читаем через публичный оверлап-реестр актора, без залезания
	// в защищённые поля ACampfire.
	for (TActorIterator<ACampfire> It(World); It; ++It)
	{
		if (Player.IsOverlappingActor(*It))
		{
			return true;
		}
	}

	// Деревня = безопасная зона (ADR-036): готовый статический запрос AVillageZone.
	if (bBlockWhenPlayerInVillage && AVillageZone::IsPointInVillage(World, Player.GetActorLocation()))
	{
		return true;
	}

	return false;
}

bool AWanderingWolfSpawner::IsPlayerNearEnemyBase(const APawn& Player) const
{
	UWorld* World = GetWorld();
	if (!World || EnemyBaseExclusionRadius <= 0.0f)
	{
		return false;
	}

	const float RadiusSq = EnemyBaseExclusionRadius * EnemyBaseExclusionRadius;
	for (TActorIterator<AMasterEnemyBase> It(World); It; ++It)
	{
		if (FVector::DistSquaredXY(Player.GetActorLocation(), It->GetActorLocation()) <= RadiusSq)
		{
			return true;
		}
	}
	return false;
}

bool AWanderingWolfSpawner::IsSpawnPointClearOfZones(const FVector& Point) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Не спавнить В деревне: враги туда всё равно не навигируют (нав-фильтр ADR-036) —
	// волк застрял бы в мирной зоне. Запас 200 см от границы.
	if (AVillageZone::IsPointInVillage(World, Point, /*Margin=*/200.0f))
	{
		return false;
	}

	if (CampfireExclusionRadius > 0.0f)
	{
		const float CampRadiusSq = CampfireExclusionRadius * CampfireExclusionRadius;
		for (TActorIterator<ACampfire> It(World); It; ++It)
		{
			if (FVector::DistSquaredXY(Point, It->GetActorLocation()) <= CampRadiusSq)
			{
				return false;
			}
		}
	}

	if (EnemyBaseExclusionRadius > 0.0f)
	{
		const float BaseRadiusSq = EnemyBaseExclusionRadius * EnemyBaseExclusionRadius;
		for (TActorIterator<AMasterEnemyBase> It(World); It; ++It)
		{
			if (FVector::DistSquaredXY(Point, It->GetActorLocation()) <= BaseRadiusSq)
			{
				return false;
			}
		}
	}

	return true;
}

bool AWanderingWolfSpawner::IsPointOnPlayerScreen(const FVector& Point) const
{
	// Тот же приём, что честность бандита-стрелка (ADR-035, AEnemyAIController): проекция
	// мировой точки на экран локального игрока. Нет камеры/вьюпорта (headless-прогон) —
	// считаем «не в кадре» и спавн не блокируем.
	const APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		return false;
	}

	int32 SizeX = 0;
	int32 SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	if (SizeX <= 0 || SizeY <= 0)
	{
		return false;
	}

	FVector2D ScreenPos;
	if (!PC->ProjectWorldLocationToScreen(Point, ScreenPos, /*bPlayerViewportRelative=*/true))
	{
		return false; // за камерой — точно не в кадре
	}

	// В кадре с запасом ScreenEdgeMargin: точка чуть за кромкой всё ещё «видима» —
	// чтобы волк не проявлялся по самому краю экрана.
	return ScreenPos.X >= -ScreenEdgeMargin && ScreenPos.X <= static_cast<float>(SizeX) + ScreenEdgeMargin
		&& ScreenPos.Y >= -ScreenEdgeMargin && ScreenPos.Y <= static_cast<float>(SizeY) + ScreenEdgeMargin;
}
