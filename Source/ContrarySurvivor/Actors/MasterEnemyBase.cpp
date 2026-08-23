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
#include "ContrarySurvivor/Controllers/EnemyAIController.h" // D7: SetLeash заспавненным врагам
#include "ContrarySurvivor/Actors/EnemyBaseTierLogic.h"     // ТЗ 22.08: чистые правила ступеней
#include "ContrarySurvivor/Components/StatsComponent.h"     // прибавка здоровья от базы (§3)
#include "ContrarySurvivor/Save/ContrarySaveGame.h"         // память базы в сейве (§1)
#include "ContrarySurvivor/Characters/PlayerCharacter.h"    // имя боевого слота сейва (статики)
#include "ContrarySurvivor/UI/BaseEntryAnnounceWidget.h"    // надпись при входе (§5)
#include "ContrarySurvivor/Components/CorpseLootComponent.h" // хранилище базы (Report1 п.11)
#include "ContrarySurvivor/ContrarySurvivor.h"              // LogQA
#include "Components/AudioComponent.h"                      // шум занятой базы (§5)
#include "Sound/SoundBase.h"
#include "Blueprint/UserWidget.h"                           // CreateWidget (надпись входа)

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

	// Визуализатор радиуса поводка (фидбек Рината 07-05): та же схема, что ActivationVisualizer,
	// но радиус = LeashRadius и другой цвет, чтобы границы не путались во вьюпорте. Отдельного
	// параметра радиуса НЕТ — сфера следует за LeashRadius (синк в OnConstruction).
	LeashVisualizer = CreateDefaultSubobject<USphereComponent>(TEXT("LeashVisualizer"));
	LeashVisualizer->SetupAttachment(SceneRoot);
	LeashVisualizer->InitSphereRadius(LeashRadius);
	LeashVisualizer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeashVisualizer->SetCanEverAffectNavigation(false);
	LeashVisualizer->ShapeColor = FColor(150, 60, 255, 255); // фиолетовый — граница поводка погони

	// Дефолтная точка спавна-образец: видимый перемещаемый маркер на базовом акторе. Смещаем от
	// центра, чтобы стрелка не сливалась с корнем. Дизайнер двигает её и добавляет ещё точек в BP.
	DefaultSpawnPoint = CreateDefaultSubobject<UEnemySpawnPointComponent>(TEXT("SpawnPoint0"));
	DefaultSpawnPoint->SetupAttachment(SceneRoot);
	DefaultSpawnPoint->SetRelativeLocation(FVector(200.0f, 0.0f, 0.0f));

	// Дефолтные классы опц. квест-предмета (editor-независимо; bSpawnQuestItem выключен по умолчанию).
	QuestItemClass = AQuestItem::StaticClass();
	PickupClass = APickup::StaticClass();

	// Шум занятой базы (ТЗ 22.08 §5 «звук раньше картинки»): компонент создаём всегда,
	// звук — мягкой ссылкой (пусто = тихо); затухание перекрываем своим радиусом слышимости.
	AmbientAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientAudio"));
	AmbientAudio->SetupAttachment(SceneRoot);
	AmbientAudio->bAutoActivate = false;
	AmbientAudio->bOverrideAttenuation = true;
	AmbientAudio->AttenuationOverrides.FalloffDistance = AmbientHearRadius;

	// Хранилище базы (Report1 п.11): «отдельное хранилище» — своё окно обыска, в группы не
	// входит; база после обыска остаётся на месте (общий выключатель убирания погашен).
	// Название и уровень для перечня ставит FillBaseStash при зачистке.
	BaseLoot = CreateDefaultSubobject<UCorpseLootComponent>(TEXT("BaseLoot"));
	BaseLoot->bStandaloneStash = true;
	BaseLoot->bSinkWhenSearched = false;

	// Награды по ступеням (ТЗ §4, дефолты по таблице владельца: 1-2 — расходники, 3+ —
	// элемент брони «получше» с ростом тира). Предметы — СТРОКАМИ таблицы DT_Items (единый
	// конфиг, ADR-075); таблица не заведена — строки не разрешатся (мешок предупредит), но
	// деньги останутся: «пусто не бывает» держится и без таблицы.
	auto MakeRewardItem = [](const TCHAR* Row, int32 Count)
	{
		FPlacedLootEntry Entry;
		Entry.ItemRow = FName(Row);
		Entry.Count = Count;
		return Entry;
	};
	TierRewards.SetNum(5);
	TierRewards[0].Money = 15.0f; // ступень 1: расходники
	TierRewards[0].Items = { MakeRewardItem(TEXT("canned_food"), 1), MakeRewardItem(TEXT("water_bottle"), 1) };
	TierRewards[1].Money = 25.0f; // ступень 2: расходников больше
	TierRewards[1].Items = { MakeRewardItem(TEXT("canned_food"), 2), MakeRewardItem(TEXT("water_bottle"), 2),
		MakeRewardItem(TEXT("bandage"), 1) };
	TierRewards[2].Money = 30.0f; // ступень 3: элемент брони
	TierRewards[2].Items = { MakeRewardItem(TEXT("armor_t1_torso"), 1), MakeRewardItem(TEXT("bandage"), 1) };
	TierRewards[3].Money = 40.0f; // ступень 4: броня получше + немного расходников
	TierRewards[3].Items = { MakeRewardItem(TEXT("armor_t2_torso"), 1), MakeRewardItem(TEXT("canned_food"), 2) };
	TierRewards[4].Money = 60.0f; // ступень 5: броня получше + расходники
	TierRewards[4].Items = { MakeRewardItem(TEXT("armor_t3_torso"), 1), MakeRewardItem(TEXT("canned_food"), 2),
		MakeRewardItem(TEXT("bandage"), 2) };
}

void AMasterEnemyBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Держим радиусы сфер-визуализаторов равными своим полям — чтобы при правке ActivationRadius/
	// LeashRadius в Details границы сразу обновлялись во вьюпорте (превью BP и размещённый актор
	// реконструируются при изменении свойства, что снова вызывает OnConstruction).
	if (ActivationVisualizer)
	{
		ActivationVisualizer->SetSphereRadius(ActivationRadius, /*bUpdateOverlaps=*/false);
	}
	if (LeashVisualizer)
	{
		LeashVisualizer->SetSphereRadius(LeashRadius, /*bUpdateOverlaps=*/false);
	}

	// Радиус слышимости базы — живой в редакторе (ТЗ §5: настраивается на BP и экземпляре).
	if (AmbientAudio)
	{
		AmbientAudio->AttenuationOverrides.FalloffDistance = AmbientHearRadius;
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

	// ТЗ 22.08 §1: память базы (ступень + время зачистки по РЕАЛЬНЫМ часам) — из сейва,
	// до взведения. Пауза не истекла — база стоит пустой (Cleared), спавна не будет.
	LoadTierStateFromSave();

	// Повторяющийся таймер — теперь НЕ гасится после активации: он же следит за зачисткой
	// и истечением паузы возрождения (машина состояний в CheckActivation).
	World->GetTimerManager().SetTimer(
		ActivationTimerHandle, this, &AMasterEnemyBase::CheckActivation,
		ActivationCheckPeriod, /*bLoop=*/true, /*FirstDelay=*/ActivationCheckPeriod);

	UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s' armed at %s, R=%.0f, tier=%d, state=%d"),
		*GetName(), *GetActorLocation().ToCompactString(), ActivationRadius,
		CurrentTier, static_cast<int32>(Occupancy));
}

void AMasterEnemyBase::CheckActivation()
{
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

	// Горизонтальная (XY) дистанция игрока до центра зоны: Z игнорируем (рельеф/высота
	// игрока не влияют). Грань ВХОДА в радиус — момент показа надписи (ТЗ §5).
	const float DistSqXY = FVector::DistSquaredXY(PlayerPawn->GetActorLocation(), GetActorLocation());
	const float RadiusSq = ActivationRadius * ActivationRadius;
	const bool bInside = DistSqXY <= RadiusSq;
	const bool bEntered = bInside && !bPlayerInsideRadius;
	bPlayerInsideRadius = bInside;

	switch (Occupancy)
	{
	case EEnemyBaseOccupancy::Dormant:
		// Взведена: вход игрока планирует спавн (пауза SpawnDelay — Nav Invoker достраивает
		// тайлы). Надпись показываем на грани входа: база занимается прямо сейчас.
		if (bInside && !bSpawnScheduled)
		{
			bSpawnScheduled = true;
			World->GetTimerManager().SetTimer(
				SpawnDelayTimerHandle, this, &AMasterEnemyBase::DoSpawn,
				FMath::Max(0.01f, SpawnDelay), /*bLoop=*/false);
			UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': player in range, spawning in %.1fs (tier %d)"),
				*GetName(), SpawnDelay, CurrentTier);
		}
		if (bEntered)
		{
			// База занимается прямо на этом входе (спавн запланирован) — надпись с фразой ступени.
			ShowEntryAnnounce(/*bBaseOccupied=*/true);
		}
		break;

	case EEnemyBaseOccupancy::Occupied:
		if (bEntered)
		{
			ShowEntryAnnounce(/*bBaseOccupied=*/true);
		}
		// ПОЛНАЯ зачистка = все заспавненные мертвы (побег/смерть игрока ничего не меняют —
		// враги-то живы, ТЗ §2).
		if (AreAllSpawnedEnemiesDead())
		{
			HandleBaseCleared();
		}
		break;

	case EEnemyBaseOccupancy::Cleared:
		// Поправка лида 22.08: и у ПУСТОЙ базы при входе показывается НАЗВАНИЕ места —
		// игрок понимает, куда пришёл; фразы ступени и цифры нет (база-то пуста).
		if (bEntered)
		{
			ShowEntryAnnounce(/*bBaseOccupied=*/false);
		}
		// Пауза по РЕАЛЬНЫМ часам (по зачищенной ступени). Переармирование ТОЛЬКО когда
		// игрок ВНЕ радиуса — враги не появляются на глазах (ТЗ §6): сам спавн потом идёт
		// штатной активацией на границе радиуса (вне видимости по конструкции).
		if (!bInside && EnemyBaseTierLogic::IsRespawnPauseElapsed(
				LastClearUtc, FDateTime::UtcNow(),
				EnemyBaseTierLogic::PauseMinutesForClearedTier(LastClearedTier, RespawnPauseMinutesPerTier)))
		{
			Occupancy = EEnemyBaseOccupancy::Dormant;
			bSpawnScheduled = false;
			UE_LOG(LogQA, Display,
				TEXT("QA: база '%s' — пауза возрождения истекла, взведена заново на ступени %d"),
				*GetName(), CurrentTier);
		}
		break;
	}
}

void AMasterEnemyBase::DoSpawn()
{
	SpawnedEnemies.Reset();
	SpawnEnemies();

	// Квест-предмет (ноутбук кв.2) — только пока базу ни разу не зачищали: возрождённая
	// база не плодит вторые ноутбуки (допущение, доложено лиду).
	if (bSpawnQuestItem && LastClearUtc.GetTicks() == 0)
	{
		SpawnQuestItem();
	}

	// Занята — только если кто-то реально заспавнился (без EnemyClass база остаётся пустой,
	// как раньше: спавн не переигрывается, лог уже написан в SpawnEnemies).
	if (SpawnedEnemies.Num() > 0)
	{
		Occupancy = EEnemyBaseOccupancy::Occupied;
		UpdateAmbientForState();
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
		// D7 (ADR-036): поводок — дом врага = центр ЭТОЙ базы, радиус — с экземпляра базы.
		// Контроллер уже существует: AutoPossessAI поссессит пешку в ходе SpawnActor.
		if (AEnemyAIController* AI = Cast<AEnemyAIController>(Enemy->GetController()))
		{
			AI->SetLeash(GetActorLocation(), LeashRadius);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("EnemyBase '%s': %s has no AEnemyAIController yet — leash NOT set (default leash from OnPossess applies)"),
				*GetName(), *Enemy->GetName());
		}

		// ТЗ 22.08 §3: прибавка здоровья ступени — ОТ БАЗЫ, обобщённо через UStatsComponent
		// (во враге не зашито; BeginPlay врага уже выставил его базовое здоровье внутри
		// SpawnActor, прибавка ложится поверх). §2/§6: число врагов от ступени НЕ растёт.
		SpawnedEnemies.Add(Enemy);
		const float HealthBonus = EnemyBaseTierLogic::HealthBonusForTier(CurrentTier, TierHealthBonus);
		if (HealthBonus > 0.0f)
		{
			if (UStatsComponent* EnemyStats = Enemy->FindComponentByClass<UStatsComponent>())
			{
				EnemyStats->InitHealth(EnemyStats->GetMaxHealth() + HealthBonus, /*bSetToMax=*/true);
				UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': %s tier %d health bonus +%.0f -> %.0f"),
					*GetName(), *Enemy->GetName(), CurrentTier, HealthBonus, EnemyStats->GetMaxHealth());
			}
		}

		UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': spawned %s at %s (navmesh=%s, leash=%.0f)"),
			*GetName(), *Enemy->GetName(), *ProjectedLoc.ToString(), bProjected ? TEXT("yes") : TEXT("floor-trace"), LeashRadius);
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
		QuestItemClass, /*ItemDropChance=*/1.0f, PickupClass, QuestItemName, QuestItemText);

	UE_LOG(LogTemp, Log, TEXT("EnemyBase '%s': quest item '%s' spawned at %s (%s)"),
		*GetName(), *QuestItemName, *Loc.ToCompactString(), Pickup ? TEXT("ok") : TEXT("FAILED"));
}

// ===========================================================================
// Возрождение и ступени (ТЗ издателя 22.08.2026)
// ===========================================================================

FText AMasterEnemyBase::BuildAnnounceText(const FText& BaseName, const FText& Separator,
	const FText& Tier2Suffix, const FText& Tier3PlusSuffix, int32 Tier)
{
	// §5: 1-я ступень — только название; 2-я — «более опытные»; 3-я и выше — «ещё более».
	const int32 ClampedTier = EnemyBaseTierLogic::ClampTier(Tier);
	const FText& Suffix = (ClampedTier == 2) ? Tier2Suffix : Tier3PlusSuffix;
	if (ClampedTier <= 1 || Suffix.IsEmpty())
	{
		return BaseName;
	}
	if (BaseName.IsEmpty())
	{
		return Suffix;
	}
	return FText::Join(Separator, TArray<FText>({ BaseName, Suffix }));
}

int32 AMasterEnemyBase::PickRewardTierIndex(const TArray<FEnemyBaseTierReward>& Rewards, int32 Tier)
{
	// §4 «пусто не бывает никогда»: запись ступени, пустая — запись первой ступени,
	// и она пустая — INDEX_NONE (вызывающий громко предупреждает).
	if (Rewards.Num() == 0)
	{
		return INDEX_NONE;
	}
	const int32 Index = FMath::Clamp(EnemyBaseTierLogic::ClampTier(Tier) - 1, 0, Rewards.Num() - 1);
	if (!Rewards[Index].IsEmpty())
	{
		return Index;
	}
	if (!Rewards[0].IsEmpty())
	{
		return 0;
	}
	return INDEX_NONE;
}

FName AMasterEnemyBase::ResolveBaseSaveId() const
{
	// Имя размещённого актора стабильно между запусками (актор с карты) — годится ключом;
	// поле BaseSaveId страхует от будущего переименования базы на карте.
	return BaseSaveId.IsNone() ? GetFName() : BaseSaveId;
}

void AMasterEnemyBase::LoadTierStateFromSave()
{
	CurrentTier = EnemyBaseTierLogic::ClampTier(InitialTier);
	LastClearedTier = 0;
	LastClearUtc = FDateTime(0);
	Occupancy = EEnemyBaseOccupancy::Dormant;

	const FString Slot = APlayerCharacter::GetDefaultSaveSlotName();
	const int32 UserIndex = APlayerCharacter::GetDefaultSaveUserIndex();
	if (!UGameplayStatics::DoesSaveGameExist(Slot, UserIndex))
	{
		return; // свежий профиль — начальная ступень
	}
	const UContrarySaveGame* Save = Cast<UContrarySaveGame>(
		UGameplayStatics::LoadGameFromSlot(Slot, UserIndex));
	if (!Save)
	{
		return;
	}

	const FName Id = ResolveBaseSaveId();
	for (const FSavedEnemyBaseState& State : Save->EnemyBaseStates)
	{
		if (State.BaseId != Id)
		{
			continue;
		}
		CurrentTier = EnemyBaseTierLogic::ClampTier(State.Tier);
		LastClearedTier = State.LastClearedTier;
		LastClearUtc = State.LastClearUtc;

		// §1: пауза не истекла — база стоит ПУСТОЙ и после перезапуска игры.
		const float PauseMinutes = EnemyBaseTierLogic::PauseMinutesForClearedTier(
			LastClearedTier, RespawnPauseMinutesPerTier);
		Occupancy = EnemyBaseTierLogic::IsRespawnPauseElapsed(LastClearUtc, FDateTime::UtcNow(), PauseMinutes)
			? EEnemyBaseOccupancy::Dormant : EEnemyBaseOccupancy::Cleared;

		UE_LOG(LogQA, Display,
			TEXT("QA: база '%s' — память из сейва: ступень %d, зачищена ступень %d, состояние %s"),
			*GetName(), CurrentTier, LastClearedTier,
			Occupancy == EEnemyBaseOccupancy::Cleared ? TEXT("пауза тикает") : TEXT("взведена"));
		// Фикс-диагностика п.13 (23.08): запись сейва ЗАКОННО главнее «Начальной ступени»
		// экземпляра (иначе возрождение ломалось бы перезапуском) — но при тестовой ручной
		// ступени это выглядит поломкой, поэтому говорим о перекрытии прямо.
		if (EnemyBaseTierLogic::ClampTier(InitialTier) != CurrentTier)
		{
			UE_LOG(LogQA, Warning,
				TEXT("QA: база '%s' — «Начальная ступень» экземпляра (%d) ПЕРЕКРЫТА памятью сейва (%d). Для теста ступени сотрите сейв («Новая игра») или смените «Идентификатор базы в сейве»"),
				*GetName(), EnemyBaseTierLogic::ClampTier(InitialTier), CurrentTier);
		}
		return;
	}
}

void AMasterEnemyBase::WriteTierStateToSave() const
{
	// Retention-паттерн (как компоненты удержания игрока): загрузили слот, правим ТОЛЬКО
	// свою запись, сохранили. Слота нет — создаём свежий объект (bHasData=false: LoadGame
	// игрока такой игнорирует, а наша память живёт).
	const FString Slot = APlayerCharacter::GetDefaultSaveSlotName();
	const int32 UserIndex = APlayerCharacter::GetDefaultSaveUserIndex();

	UContrarySaveGame* Save = Cast<UContrarySaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, UserIndex));
	if (!Save)
	{
		Save = Cast<UContrarySaveGame>(
			UGameplayStatics::CreateSaveGameObject(UContrarySaveGame::StaticClass()));
	}
	if (!Save)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: база '%s' — не создать объект сейва, ступень НЕ записана"), *GetName());
		return;
	}

	const FName Id = ResolveBaseSaveId();
	FSavedEnemyBaseState* Record = Save->EnemyBaseStates.FindByPredicate(
		[&Id](const FSavedEnemyBaseState& State) { return State.BaseId == Id; });
	if (!Record)
	{
		Record = &Save->EnemyBaseStates.AddDefaulted_GetRef();
		Record->BaseId = Id;
	}
	Record->Tier = CurrentTier;
	Record->LastClearedTier = LastClearedTier;
	Record->LastClearUtc = LastClearUtc;

	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, Slot, UserIndex);
	UE_LOG(LogQA, Display, TEXT("QA: база '%s' — ступень %d записана в сейв: %s"),
		*GetName(), CurrentTier, bOk ? TEXT("OK") : TEXT("FAIL"));
}

bool AMasterEnemyBase::AreAllSpawnedEnemiesDead() const
{
	if (SpawnedEnemies.Num() == 0)
	{
		return false; // ещё никого не спавнили — «зачищать» нечего
	}
	for (const TWeakObjectPtr<ACharacter>& Ptr : SpawnedEnemies)
	{
		const ACharacter* Enemy = Ptr.Get();
		if (!Enemy)
		{
			continue; // труп исчез по таймеру — мёртв
		}
		const UStatsComponent* EnemyStats = Enemy->FindComponentByClass<UStatsComponent>();
		if (!EnemyStats || !EnemyStats->IsDead())
		{
			// Без статов — считаем живым: лучше не зачесть зачистку, чем зачесть лишнюю.
			return false;
		}
	}
	return true;
}

void AMasterEnemyBase::HandleBaseCleared()
{
	// §2: ступень растёт ТОЛЬКО за полную зачистку; после пятой — третья (цикл 3→4→5→3).
	const int32 ClearedTier = EnemyBaseTierLogic::ClampTier(CurrentTier);
	LastClearedTier = ClearedTier;
	LastClearUtc = FDateTime::UtcNow(); // РЕАЛЬНЫЕ часы (§1)
	CurrentTier = EnemyBaseTierLogic::NextTierAfterClear(ClearedTier);
	Occupancy = EEnemyBaseOccupancy::Cleared;
	SpawnedEnemies.Reset();

	WriteTierStateToSave();
	FillBaseStash(ClearedTier);
	UpdateAmbientForState(); // база опустела — тишина

	UE_LOG(LogQA, Display,
		TEXT("QA: база '%s' ЗАЧИЩЕНА (ступень %d) — следующая ступень %d, пауза %.0f мин реального времени"),
		*GetName(), ClearedTier, CurrentTier,
		EnemyBaseTierLogic::PauseMinutesForClearedTier(ClearedTier, RespawnPauseMinutesPerTier));
}

void AMasterEnemyBase::FillBaseStash(int32 ClearedTier)
{
	// Report1 п.11 (Ринат: «Игрок обыскивает именно базу (логово)… лут встроен в сам класс
	// базы»): награда кладётся в хранилище-компонент, мешок в центре больше не спавнится.
	UWorld* World = GetWorld();
	if (!World || !BaseLoot)
	{
		return;
	}

	const int32 RewardIndex = PickRewardTierIndex(TierRewards, ClearedTier);
	if (RewardIndex == INDEX_NONE)
	{
		// §4 «пусто не бывает» — держится дефолтами конструктора; сюда попадают только
		// руками опустошённые настройки, о чём говорим громко.
		UE_LOG(LogQA, Warning,
			TEXT("QA: база '%s' — награда за зачистку ПУСТАЯ (все записи ступеней пусты) — хранилище не наполнено, заполните «Награда по ступеням»"),
			*GetName());
		return;
	}
	const FEnemyBaseTierReward& Reward = TierRewards[RewardIndex];

	// Предметы — общий путь списка (строка DT_Items/класс + количество, ADR-076 п.10).
	TArray<AMasterInventoryItem*> Items =
		APickup::SpawnLootEntries(World, Reward.Items, GetActorLocation(), GetName());

	// Перечень окна обыска (Report1 п.14): название базы + уровень — окно выделит имя
	// отдельным кубиком и припишет «Ур. N». InitLoot ЗАМЕЩАЕТ содержимое: не забранное с
	// прошлой зачистки пропадает (допущение — новая зачистка, новый лут). Регистрация в
	// реестре обыскиваемых даёт подсказку «Обыскать» у центра базы.
	BaseLoot->SearchObjectName = BaseDisplayName;
	BaseLoot->StashLevel = ClearedTier;
	BaseLoot->InitLoot(Reward.Money, Items, /*bRegisterSearchable=*/true);

	UE_LOG(LogQA, Display,
		TEXT("QA: база '%s' — хранилище наполнено наградой ступени %d (%d предметов, деньги %.0f)"),
		*GetName(), ClearedTier, Items.Num(), Reward.Money);
}

void AMasterEnemyBase::ShowEntryAnnounce(bool bBaseOccupied)
{
	// Фикс п.13 отчёта 23.08 («выставил ступень — надписи нет»): пустое «Название места»
	// раньше глотало надпись ЦЕЛИКОМ. Теперь у ЗАНЯТОЙ базы фраза ступени показывается и
	// без названия (BuildAnnounceText отдаёт одну фразу), а пустое имя один раз громко
	// уходит в журнал — чтобы дырку в настройке BP было видно сразу.
	if (BaseDisplayName.IsEmpty() && !bAnnounceNameWarned)
	{
		bAnnounceNameWarned = true;
		UE_LOG(LogQA, Warning,
			TEXT("QA: база '%s' — «Название места» (EnemyBase|Надпись) НЕ заполнено в BP/экземпляре: у пустой базы надписи не будет вовсе, у занятой — только фраза ступени без имени"),
			*GetName());
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (Now - LastAnnounceTime < AnnounceCooldown)
	{
		return; // антиспам повторных входов
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}
	if (!AnnounceWidget.IsValid())
	{
		// П.0 ADR-077: класс окна — слот (WBP_BaseAnnounce со стилем дизайнера); пусто —
		// голый C++-класс с запасным кодовым деревом.
		UClass* WidgetClass = AnnounceWidgetClass
			? static_cast<UClass*>(AnnounceWidgetClass) : UBaseEntryAnnounceWidget::StaticClass();
		UBaseEntryAnnounceWidget* Widget = CreateWidget<UBaseEntryAnnounceWidget>(PC, WidgetClass);
		if (!Widget)
		{
			return;
		}
		Widget->AddToViewport(/*ZOrder=*/15); // над постоянными панелями (5), под модалками (30)
		AnnounceWidget = Widget;
	}
	LastAnnounceTime = Now;

	// Поправка лида 22.08: пустая база — ТОЛЬКО название; занятая — строка по ступени, и
	// с 3-й рядом номер «полупрозрачной цифрой, без скобок» вторым планом (ТЗ §5).
	const FText Line = bBaseOccupied
		? BuildAnnounceText(BaseDisplayName, AnnounceSeparator, Tier2Suffix, Tier3PlusSuffix, CurrentTier)
		: BaseDisplayName;
	if (Line.IsEmpty())
	{
		return; // показывать нечего (пустая база без названия)
	}
	const bool bShowDigit = bBaseOccupied && CurrentTier >= 3;
	AnnounceWidget->ShowAnnounce(Line,
		FText::AsNumber(CurrentTier, &FNumberFormattingOptions::DefaultNoGrouping()),
		bShowDigit, AnnounceDuration);

	UE_LOG(LogQA, Display, TEXT("QA: база '%s' — надпись входа показана (ступень %d, %s)"),
		*GetName(), CurrentTier, bBaseOccupied ? TEXT("занята") : TEXT("пустая, только название"));
}

void AMasterEnemyBase::UpdateAmbientForState()
{
	if (!AmbientAudio)
	{
		return;
	}
	AmbientAudio->AttenuationOverrides.FalloffDistance = AmbientHearRadius;

	if (Occupancy == EEnemyBaseOccupancy::Occupied && !AmbientSound.IsNull())
	{
		USoundBase* Sound = AmbientSound.LoadSynchronous();
		if (!Sound)
		{
			UE_LOG(LogQA, Warning,
				TEXT("QA: база '%s' — звук базы не загрузился ('%s'); если в редакторе он есть, а в паке нет — проверь список обязательной упаковки"),
				*GetName(), *AmbientSound.ToString());
			return;
		}
		if (AmbientAudio->Sound != Sound)
		{
			AmbientAudio->SetSound(Sound);
		}
		// §5: «чем выше ступень, тем больше голосов слышно» — громкость по ступени.
		AmbientAudio->SetVolumeMultiplier(FMath::Max(0.0f,
			EnemyBaseTierLogic::PerTierValue(CurrentTier, TierAmbientVolume, 1.0f)));
		if (!AmbientAudio->IsPlaying())
		{
			AmbientAudio->Play();
		}
	}
	else if (AmbientAudio->IsPlaying())
	{
		AmbientAudio->Stop();
	}
}
