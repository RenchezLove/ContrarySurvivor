// Fill out your copyright notice in the Description page of Project Settings.

#include "QuestBarrier.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"                 // TActorIterator: герой ищется по классу
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "NavAreas/NavArea_Null.h"       // закрытый шлагбаум вырезает проезд из навигационной сетки
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA

namespace
{
	// Сетка по умолчанию: движковый куб 100 х 100 х 100 см с началом координат в центре.
	const TCHAR* DefaultCubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const float DefaultCubeSize = 100.0f;

	bool IsDefaultCube(const UStaticMeshComponent* Comp)
	{
		const UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
		return Mesh && Mesh->GetPathName() == DefaultCubePath;
	}

	// Куб растягиваем под размеры; у своей сетки масштаб не трогаем — кроме случая, когда на ней
	// остался масштаб от куба (сетку только что заменили): тогда возвращаем единичный.
	void FitPart(UStaticMeshComponent* Comp, const FVector& CubeLocation, const FVector& CubeSize,
		const FVector& OwnMeshLocation)
	{
		if (!Comp)
		{
			return;
		}
		const FVector CubeScale = CubeSize / DefaultCubeSize;
		if (IsDefaultCube(Comp))
		{
			Comp->SetRelativeLocation(CubeLocation);
			Comp->SetRelativeScale3D(CubeScale);
			return;
		}
		Comp->SetRelativeLocation(OwnMeshLocation);
		if (Comp->GetRelativeScale3D().Equals(CubeScale, KINDA_SMALL_NUMBER))
		{
			Comp->SetRelativeScale3D(FVector::OneVector);
		}
	}
}

AQuestBarrier::AQuestBarrier()
{
	// Тик нужен только пока стрела движется — включается на время подъёма и опускания.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(DefaultCubePath);

	// Стойка и стрела — только вид: не пускает и влияет на навигацию одна коробка-преграда.
	PostMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostMesh"));
	PostMesh->SetupAttachment(SceneRoot);
	PostMesh->SetMobility(EComponentMobility::Static);
	PostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PostMesh->SetCanEverAffectNavigation(false);

	BoomPivot = CreateDefaultSubobject<USceneComponent>(TEXT("BoomPivot"));
	BoomPivot->SetupAttachment(SceneRoot);
	BoomPivot->SetMobility(EComponentMobility::Movable);

	BoomMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoomMesh"));
	BoomMesh->SetupAttachment(BoomPivot);
	BoomMesh->SetMobility(EComponentMobility::Movable);
	BoomMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoomMesh->SetCanEverAffectNavigation(false);

	if (CubeFinder.Succeeded())
	{
		PostMesh->SetStaticMesh(CubeFinder.Object);
		BoomMesh->SetStaticMesh(CubeFinder.Object);
	}

	// Преграда: блокирует только персонажей (и транспорт); пули, обзор и камера проходят.
	Blocker = CreateDefaultSubobject<UBoxComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(SceneRoot);
	Blocker->SetMobility(EComponentMobility::Static);
	Blocker->SetCollisionObjectType(ECC_WorldStatic);
	Blocker->SetCollisionResponseToAllChannels(ECR_Ignore);
	Blocker->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Blocker->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Block);
	Blocker->SetGenerateOverlapEvents(false);
	// Навигация: коробка уходит в навигационную сетку не геометрией, а областью «прохода нет»
	// (ShapeComponent.cpp:159-175, UE 5.5) — её включение и выключение перестраивает только
	// плитки вокруг проезда. Работает при динамической сетке уровня (в том числе в режиме
	// «только динамические области»); при статической сетке врагов держит одна коллизия.
	Blocker->bDynamicObstacle = true;
	Blocker->SetAreaClassOverride(UNavArea_Null::StaticClass());

	LayoutParts();
	ApplyBlocker();
	ApplyBoomPose();
}

bool AQuestBarrier::ShouldBeOpen(const TArray<FQuest>& Journal, FName TakenQuestId, FName TurnedInQuestId)
{
	for (const FQuest& Quest : Journal)
	{
		if (Quest.QuestId.IsNone())
		{
			continue;
		}
		// Принято: идёт, выполнено или уже сдано («только предложено» — ещё нет).
		if (Quest.QuestId == TakenQuestId && Quest.State != EQuestState::NotStarted)
		{
			return true;
		}
		if (Quest.QuestId == TurnedInQuestId && Quest.State == EQuestState::TurnedIn)
		{
			return true;
		}
	}
	return false;
}

bool AQuestBarrier::ShouldLetHeroOut(float InsideDepth, float Distance, bool bWasLettingOut,
	float HalfThickness, float Radius, float OutsideClearance)
{
	if (Distance > Radius)
	{
		return false; // далеко от шлагбаума — выпускать некого
	}
	if (InsideDepth > HalfThickness)
	{
		return true; // герой внутри, преграда между ним и выездом
	}
	// В проезде или только что вышел: держим открытым, пока не отошёл наружу на запас.
	return bWasLettingOut && InsideDepth > -(HalfThickness + OutsideClearance);
}

bool AQuestBarrier::UpdateLetHeroOut(const APlayerCharacter* Player)
{
	if (!bLetHeroOutFromInside || !Player)
	{
		bLettingHeroOut = false;
		return false;
	}
	const FVector Offset = Player->GetActorLocation() - GetActorLocation();
	const FVector Inward = GetActorForwardVector().GetSafeNormal2D() * (bInsideIsForward ? 1.0f : -1.0f);
	const float InsideDepth = FVector::DotProduct(FVector(Offset.X, Offset.Y, 0.0f), Inward);
	const float Distance = FVector(Offset.X, Offset.Y, 0.0f).Size();
	const bool bNow = ShouldLetHeroOut(InsideDepth, Distance, bLettingHeroOut,
		BlockerThickness * 0.5f, LetOutRadius, LetOutClearance);
	if (bNow != bLettingHeroOut)
	{
		UE_LOG(LogQA, Display, TEXT("QA: barrier %s let-hero-out %s (depth %.0f, dist %.0f)"),
			*GetName(), bNow ? TEXT("ON") : TEXT("OFF"), InsideDepth, Distance);
	}
	bLettingHeroOut = bNow;
	return bNow;
}

void AQuestBarrier::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Размеры — с размещённого экземпляра (конструктор видел только значения класса).
	LayoutParts();
	ApplyBlocker();
	ApplyBoomPose();
}

void AQuestBarrier::LayoutParts()
{
	const float HalfWidth = PassageWidth * 0.5f;

	if (Blocker)
	{
		Blocker->SetBoxExtent(FVector(BlockerThickness * 0.5f, HalfWidth, BlockerHeight * 0.5f));
		Blocker->SetRelativeLocation(FVector(0.0f, 0.0f, BlockerHeight * 0.5f));
	}
	if (BoomPivot)
	{
		BoomPivot->SetRelativeLocation(FVector(0.0f, -HalfWidth, BoomHeight));
	}

	// Стойка: куб — столбик от земли до оси стрелы; своя сетка стоит началом координат на земле.
	const float PostThickness = DefaultBarThickness * 1.5f;
	FitPart(PostMesh,
		FVector(0.0f, -HalfWidth, BoomHeight * 0.5f),
		FVector(PostThickness, PostThickness, FMath::Max(1.0f, BoomHeight)),
		FVector(0.0f, -HalfWidth, 0.0f));

	// Стрела: куб — брусок от оси на всю ширину проезда; своя сетка начинается в оси.
	FitPart(BoomMesh,
		FVector(0.0f, HalfWidth, 0.0f),
		FVector(DefaultBarThickness, PassageWidth, DefaultBarThickness),
		FVector::ZeroVector);
}

void AQuestBarrier::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		RefreshNow();
		World->GetTimerManager().SetTimer(CheckTimer, this, &AQuestBarrier::RefreshNow,
			FMath::Max(0.1f, CheckInterval), /*bLoop=*/true);
	}
}

void AQuestBarrier::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromJournal();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CheckTimer);
	}
	Super::EndPlay(EndPlayReason);
}

void AQuestBarrier::BindToPlayerJournal(APlayerCharacter* Player)
{
	UQuestComponent* Journal = Player ? Player->GetQuests() : nullptr;
	if (Journal == BoundJournal.Get())
	{
		return;
	}
	UnbindFromJournal();
	if (Journal)
	{
		Journal->OnQuestChanged.AddUniqueDynamic(this, &AQuestBarrier::HandleQuestChanged);
		JournalRestoredHandle = Journal->OnJournalRestored.AddUObject(this, &AQuestBarrier::HandleJournalRestored);
		BoundJournal = Journal;
	}
}

void AQuestBarrier::UnbindFromJournal()
{
	if (UQuestComponent* Journal = BoundJournal.Get())
	{
		Journal->OnQuestChanged.RemoveDynamic(this, &AQuestBarrier::HandleQuestChanged);
		Journal->OnJournalRestored.Remove(JournalRestoredHandle);
	}
	JournalRestoredHandle.Reset();
	BoundJournal.Reset();
}

void AQuestBarrier::HandleQuestChanged(const FQuest& Quest)
{
	RefreshNow();
}

void AQuestBarrier::HandleJournalRestored()
{
	RefreshNow();
}

void AQuestBarrier::RefreshNow()
{
	UWorld* World = GetWorld();
	APlayerCharacter* Player = nullptr;
	if (World)
	{
		for (TActorIterator<APlayerCharacter> It(World); It; ++It)
		{
			Player = *It;
			break;
		}
	}
	BindToPlayerJournal(Player);

	const UQuestComponent* Journal = BoundJournal.Get();
	if (!Journal)
	{
		return; // героя ещё нет — решать рано, шлагбаум остаётся закрытым
	}

	const bool bByQuests = ShouldBeOpen(Journal->GetQuests(), OpenWhenTakenQuestId, OpenWhenTurnedInQuestId);
	// Выпуск изнутри считается и при открытом по заданиям — чтобы латч не залип со старым значением.
	const bool bLetOut = UpdateLetHeroOut(Player);
	SetOpen(bByQuests || bLetOut, /*bInstant=*/!bResolvedOnce);
	bResolvedOnce = true;
}

void AQuestBarrier::SetOpen(bool bNewOpen, bool bInstant)
{
	if (bInstant)
	{
		bOpen = bNewOpen;
		OpenAlpha = bOpen ? 1.0f : 0.0f;
		ApplyBlocker();
		ApplyBoomPose();
		SetActorTickEnabled(false);
		return;
	}
	if (bNewOpen == bOpen)
	{
		return;
	}

	bOpen = bNewOpen;
	// Проход открывается и закрывается сразу: герой не ждёт, пока стрела доедет.
	ApplyBlocker();
	UE_LOG(LogQA, Display, TEXT("QA: barrier %s -> %s"), *GetName(), bOpen ? TEXT("OPEN") : TEXT("CLOSED"));

	if (OpenTime <= KINDA_SMALL_NUMBER)
	{
		OpenAlpha = bOpen ? 1.0f : 0.0f;
		ApplyBoomPose();
		return;
	}
	SetActorTickEnabled(true);
}

void AQuestBarrier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bOpen ? 1.0f : 0.0f;
	OpenAlpha = (OpenTime > KINDA_SMALL_NUMBER)
		? FMath::FInterpConstantTo(OpenAlpha, Target, DeltaSeconds, 1.0f / OpenTime)
		: Target;
	ApplyBoomPose();

	if (FMath::IsNearlyEqual(OpenAlpha, Target))
	{
		OpenAlpha = Target;
		SetActorTickEnabled(false);
	}
}

void AQuestBarrier::ApplyBlocker()
{
	if (!Blocker)
	{
		return;
	}
	Blocker->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	Blocker->SetCanEverAffectNavigation(!bOpen);
}

void AQuestBarrier::ApplyBoomPose()
{
	if (!BoomPivot)
	{
		return;
	}
	// Стрела тянется вдоль +Y; отрицательный крен поднимает её конец вверх
	// (RotationTranslationMatrix.h: ось Y = (.., CR, -SR) — положительный крен опускает +Y).
	const float Eased = FMath::SmoothStep(0.0f, 1.0f, OpenAlpha);
	BoomPivot->SetRelativeRotation(FRotator(0.0f, 0.0f, -OpenAngle * Eased));
}
