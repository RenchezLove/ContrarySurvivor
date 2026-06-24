// Fill out your copyright notice in the Description page of Project Settings.

#include "MasterEnemyBase.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "ContrarySurvivor/Subsystems/SpawnPlacementUtils.h" // floor-trace/ResolveSpawnZ (переиспользуем)
#include "Pickup.h"        // пикап-носитель квест-предмета (тот же каталог Actors/)
#include "AQuestItem.h"    // дефолтный класс квест-предмета «Ноутбук»

AMasterEnemyBase::AMasterEnemyBase()
{
	// Тик зоне не нужен — активацию ведём таймером (как прежние сабсистемы).
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Дефолтные классы опц. квест-предмета (editor-независимо; bSpawnQuestItem выключен по умолчанию).
	QuestItemClass = AQuestItem::StaticClass();
	PickupClass = APickup::StaticClass();
}

void AMasterEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	// Зона ждёт приближения игрока — повторяющийся таймер проверки дистанции (как сабсистемы).
	World->GetTimerManager().SetTimer(
		ActivationTimerHandle, this, &AMasterEnemyBase::CheckActivation,
		ActivationCheckPeriod, /*bLoop=*/true, /*FirstDelay=*/ActivationCheckPeriod);

	UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s' armed at %s, R=%.0f (waits for player approach)"),
		*GetName(), *GetActorLocation().ToCompactString(), ActivationRadius);
}

void AMasterEnemyBase::CheckActivation()
{
	if (bActivated)
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

	// Горизонтальная (XY) дистанция игрока до центра зоны: Z игнорируем (рельеф/высота игрока
	// не влияют на триггер активации).
	const float DistSqXY = FVector::DistSquaredXY(PlayerPawn->GetActorLocation(), GetActorLocation());
	const float RadiusSq = ActivationRadius * ActivationRadius;
	if (DistSqXY > RadiusSq)
	{
		return;
	}

	// Активация — одноразово, сразу гасим таймер проверки (CheckActivation больше не перезапланирует).
	bActivated = true;
	World->GetTimerManager().ClearTimer(ActivationTimerHandle);

	// НЕ спавним мгновенно: даём Nav Invoker на игроке SpawnDelay секунд достроить навмеш-тайлы
	// вокруг зоны (иначе враги сядут navmesh=floor-trace и не навигируют). Затем DoSpawn.
	World->GetTimerManager().SetTimer(
		SpawnDelayTimerHandle, this, &AMasterEnemyBase::DoSpawn, FMath::Max(0.01f, SpawnDelay), /*bLoop=*/false);

	UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': player in range, spawning in %.1fs"), *GetName(), SpawnDelay);
}

void AMasterEnemyBase::DoSpawn()
{
	SpawnEnemies();
	if (bSpawnQuestItem)
	{
		SpawnQuestItem();
	}
}

void AMasterEnemyBase::SpawnEnemies()
{
	UWorld* World = GetWorld();
	if (!World || !EnemyClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyBase '%s': spawn skipped (no World or EnemyClass not set in BP)"), *GetName());
		return;
	}

	const int32 Count = FMath::Max(1, NumToSpawn);
	const FVector Center = GetActorLocation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	int32 Spawned = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		// Раскладываем врагов равномерно по кругу вокруг центра зоны.
		const float AngleRad = FMath::DegreesToRadians((360.0f / Count) * i);
		FVector DesiredLoc = Center + FVector(
			FMath::Cos(AngleRad) * SpreadRadius,
			FMath::Sin(AngleRad) * SpreadRadius,
			0.0f);

		// Проецируем XY на навмеш, чтобы враг встал в проходимой точке (не в стволе/стене).
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
			World, ProjectedLoc.X, ProjectedLoc.Y, /*ZOffset=*/90.0f, TEXT("EnemyBase"));

		// Разворачиваем врага лицом к центру зоны.
		const FRotator SpawnRot = (Center - ProjectedLoc).Rotation();
		ACharacter* Enemy = World->SpawnActor<ACharacter>(
			EnemyClass, ProjectedLoc, FRotator(0.0f, SpawnRot.Yaw, 0.0f), SpawnParams);

		if (Enemy)
		{
			++Spawned;
			UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': spawned %s at %s (navmesh=%s)"),
				*GetName(), *Enemy->GetName(), *ProjectedLoc.ToString(), bProjected ? TEXT("yes") : TEXT("floor-trace"));
		}
	}

	UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': activated, spawned %d/%d enemies"), *GetName(), Spawned, Count);
}

void AMasterEnemyBase::SpawnQuestItem()
{
	UWorld* World = GetWorld();
	if (!World || !QuestItemClass)
	{
		return;
	}

	// Квест-предмет кладём в центр зоны. XY проецируем на навмеш (в проходимой точке), высоту —
	// трассой до пола (как враги). Чуть выше пола (+20), как «лут на земле».
	FVector Loc = GetActorLocation();
	FVector ProjectedOut;
	const bool bProjected = UNavigationSystemV1::K2_ProjectPointToNavigation(
		World, Loc, ProjectedOut, /*NavData=*/nullptr, /*FilterClass=*/nullptr,
		FVector(600.0f, 600.0f, 600.0f));
	if (bProjected)
	{
		Loc.X = ProjectedOut.X;
		Loc.Y = ProjectedOut.Y;
	}
	Loc.Z = SpawnPlacement::ResolveSpawnZ(World, Loc.X, Loc.Y, /*ZOffset=*/20.0f, TEXT("QuestItem"));

	// Пикап-носитель с гарантированным предметом (chance=1.0), без денег. Имя предмета = QuestItemName
	// (совпадает с RequiredItemName квеста старосты). Подбор — по E (как любой пикап).
	APickup* Pickup = APickup::DropLoot(World, Loc, /*Money=*/0.0f,
		QuestItemClass, /*ItemDropChance=*/1.0f, PickupClass, QuestItemName);

	UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': quest item '%s' spawned at %s (%s)"),
		*GetName(), *QuestItemName, *Loc.ToCompactString(), Pickup ? TEXT("ok") : TEXT("FAILED"));
}
