// Fill out your copyright notice in the Description page of Project Settings.

#include "MasterEnemyBase.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h" // визуализатор радиуса активации (каркас-сфера во вьюпорте)
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "ContrarySurvivor/Subsystems/SpawnPlacementUtils.h" // floor-trace/ResolveSpawnZ (переиспользуем)
#include "Pickup.h"        // пикап-носитель квест-предмета (тот же каталог Actors/)
#include "AQuestItem.h"    // дефолтный класс квест-предмета «Ноутбук»
#include "EnemySpawnPointComponent.h" // видимые/перемещаемые в BP точки спавна

AMasterEnemyBase::AMasterEnemyBase()
{
	// Тик зоне не нужен — активацию ведём таймером (как прежние сабсистемы).
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Визуализатор радиуса активации (фидбек Рината): каркас-сфера радиусом ActivationRadius,
	// видимая во вьюпорте редактора и скрытая в игре (bHiddenInGame у UShapeComponent = true).
	// Это чисто визуальная подсказка — НЕ триггер: активацию ведёт таймер+XY-дистанция (CheckActivation),
	// поэтому коллизию и влияние на навмеш отключаем. Радиус синхронизируется в OnConstruction.
	ActivationVisualizer = CreateDefaultSubobject<USphereComponent>(TEXT("ActivationVisualizer"));
	ActivationVisualizer->SetupAttachment(SceneRoot);
	ActivationVisualizer->InitSphereRadius(ActivationRadius);
	ActivationVisualizer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ActivationVisualizer->SetCanEverAffectNavigation(false);
	ActivationVisualizer->ShapeColor = FColor(255, 140, 0, 255); // оранжевый — заметная граница спавна

	// Дефолтная точка спавна-образец: видимый перемещаемый маркер на базовом акторе. Смещаем от
	// центра, чтобы стрелка не сливалась с корнем. Дизайнер двигает её и добавляет ещё точек в BP.
	DefaultSpawnPoint = CreateDefaultSubobject<UEnemySpawnPointComponent>(TEXT("SpawnPoint0"));
	DefaultSpawnPoint->SetupAttachment(SceneRoot);
	DefaultSpawnPoint->SetRelativeLocation(FVector(200.0f, 0.0f, 0.0f));

	// Дефолтные классы опц. квест-предмета (editor-независимо; bSpawnQuestItem выключен по умолчанию).
	QuestItemClass = AQuestItem::StaticClass();
	PickupClass = APickup::StaticClass();
}

void AMasterEnemyBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Держим радиус сферы-визуализатора равным ActivationRadius — чтобы при правке ActivationRadius
	// в Details граница сразу обновлялась во вьюпорте (превью BP и размещённый актор реконструируются
	// при изменении свойства, что снова вызывает OnConstruction).
	if (ActivationVisualizer)
	{
		ActivationVisualizer->SetSphereRadius(ActivationRadius, /*bUpdateOverlaps=*/false);
	}
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

void AMasterEnemyBase::CollectSpawnPointTransforms(TArray<FTransform>& OutTransforms) const
{
	OutTransforms.Reset();

	// Берём ВСЕ размещённые точки спавна актора (дефолтную из C++ + добавленные дизайнером в BP).
	// Тип-маркер UEnemySpawnPointComponent гарантирует, что не зацепим меш базы/триггеры/прочее.
	TArray<UEnemySpawnPointComponent*> Points;
	GetComponents<UEnemySpawnPointComponent>(Points);
	for (const UEnemySpawnPointComponent* Point : Points)
	{
		if (Point)
		{
			OutTransforms.Add(Point->GetComponentTransform());
		}
	}
}

void AMasterEnemyBase::SpawnOneEnemy(const FTransform& SpawnTransform)
{
	UWorld* World = GetWorld();
	if (!World || !EnemyClass)
	{
		return;
	}

	const FVector DesiredLoc = SpawnTransform.GetLocation();

	// Проецируем XY на навмеш, чтобы враг встал в проходимой точке (не в стволе/стене).
	FVector ProjectedLoc = DesiredLoc;
	FVector ProjectedOut;
	const bool bProjected = UNavigationSystemV1::K2_ProjectPointToNavigation(
		World, DesiredLoc, ProjectedOut, /*NavData=*/nullptr, /*FilterClass=*/nullptr,
		FVector(600.0f, 600.0f, 600.0f));
	if (bProjected)
	{
		// Высоту НЕ берём от навмеш-проекции: трасса до пола в этой XY (надёжный Z).
		ProjectedLoc.X = ProjectedOut.X;
		ProjectedLoc.Y = ProjectedOut.Y;
	}
	ProjectedLoc.Z = SpawnPlacement::ResolveSpawnZ(
		World, ProjectedLoc.X, ProjectedLoc.Y, /*ZOffset=*/90.0f, TEXT("EnemyBase"));

	// Поворот врага = Yaw точки спавна (куда смотрит стрелка маркера; дизайнер задаёт направление).
	const float SpawnYaw = SpawnTransform.GetRotation().Rotator().Yaw;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	ACharacter* Enemy = World->SpawnActor<ACharacter>(
		EnemyClass, ProjectedLoc, FRotator(0.0f, SpawnYaw, 0.0f), SpawnParams);

	if (Enemy)
	{
		UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': spawned %s at %s (navmesh=%s)"),
			*GetName(), *Enemy->GetName(), *ProjectedLoc.ToString(), bProjected ? TEXT("yes") : TEXT("floor-trace"));
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

	// Источник позиций — размещённые в BP точки спавна (задача Рината: позиции из компонентов).
	TArray<FTransform> SpawnTransforms;
	CollectSpawnPointTransforms(SpawnTransforms);

	if (SpawnTransforms.Num() > 0)
	{
		// ADR-029: число врагов НЕЗАВИСИМО от числа точек — спавним min(NumToSpawn, число точек)
		// врагов в СЛУЧАЙНОМ подмножестве точек. Перемешиваем (Fisher–Yates) и берём первые N.
		for (int32 i = SpawnTransforms.Num() - 1; i > 0; --i)
		{
			SpawnTransforms.Swap(i, FMath::RandRange(0, i));
		}

		const int32 Count = FMath::Min(FMath::Max(1, NumToSpawn), SpawnTransforms.Num());
		for (int32 i = 0; i < Count; ++i)
		{
			SpawnOneEnemy(SpawnTransforms[i]);
		}
		UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': activated, spawned %d enemies (NumToSpawn=%d, points=%d)"),
			*GetName(), Count, NumToSpawn, SpawnTransforms.Num());
		return;
	}

	// FALLBACK: точек спавна нет — старое поведение (NumToSpawn врагов по кругу вокруг центра).
	const int32 Count = FMath::Max(1, NumToSpawn);
	const FVector Center = GetActorLocation();
	for (int32 i = 0; i < Count; ++i)
	{
		const float AngleRad = FMath::DegreesToRadians((360.0f / Count) * i);
		const FVector DesiredLoc = Center + FVector(
			FMath::Cos(AngleRad) * SpreadRadius,
			FMath::Sin(AngleRad) * SpreadRadius,
			0.0f);
		// Лицом к центру зоны (как раньше для круговой раскладки).
		const float Yaw = (Center - DesiredLoc).Rotation().Yaw;
		SpawnOneEnemy(FTransform(FRotator(0.0f, Yaw, 0.0f), DesiredLoc));
	}
	UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': activated, spawned %d enemies (fallback circle, no spawn points)"),
		*GetName(), Count);
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
