// Fill out your copyright notice in the Description page of Project Settings.

#include "VillageZone.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "NavModifierComponent.h"
#include "ContrarySurvivor/Navigation/NavArea_Village.h"
#include "Engine/World.h"

TArray<TWeakObjectPtr<AVillageZone>> AVillageZone::ActiveZones;

AVillageZone::AVillageZone()
{
	// Тик не нужен: зона статична, запросы к ней делают ИИ-контроллеры.
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Видимая рамка границы (как ActivationVisualizer у AMasterEnemyBase): рисуется во
	// вьюпорте редактора, скрыта в игре, коллизии и влияния на навигацию не имеет —
	// иначе бокс сам бы стал источником границ нав-модификатора (см. VillageZone.h).
	ZoneBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneBox"));
	ZoneBox->SetupAttachment(SceneRoot);
	ZoneBox->InitBoxExtent(ZoneExtent);
	ZoneBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneBox->SetGenerateOverlapEvents(false);
	ZoneBox->SetCanEverAffectNavigation(false);
	ZoneBox->ShapeColor = FColor(80, 200, 120, 255); // зелёная граница «безопасной» деревни

	// Нав-модификатор: без коллизионных компонентов у владельца его границы = FailsafeExtent
	// вокруг актора (NavModifierComponent.cpp:185-189, UE 5.5) — синхронизируется в OnConstruction.
	NavModifier = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifier"));
	NavModifier->AreaClass = UNavArea_Village::StaticClass();
	NavModifier->FailsafeExtent = ZoneExtent;
}

void AVillageZone::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Один knob (ZoneExtent) правит и видимой рамкой, и объёмом нав-модификатора —
	// граница в редакторе всегда совпадает с границей навигации (директива Рината 06-25:
	// параметры реально влияют на визуал экземпляра сразу при правке в Details).
	if (ZoneBox)
	{
		ZoneBox->SetBoxExtent(ZoneExtent, /*bUpdateOverlaps=*/false);
	}
	if (NavModifier)
	{
		NavModifier->FailsafeExtent = ZoneExtent;
	}
}

void AVillageZone::BeginPlay()
{
	Super::BeginPlay();
	ActiveZones.AddUnique(this);

	UE_LOG(LogTemp, Log, TEXT("VillageZone '%s' active at %s, extent %s"),
		*GetName(), *GetActorLocation().ToCompactString(), *ZoneExtent.ToCompactString());
}

void AVillageZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveZones.Remove(this);
	Super::EndPlay(EndPlayReason);
}

bool AVillageZone::ContainsPoint(const FVector& Point, float Margin) const
{
	if (!ZoneBox)
	{
		return false;
	}

	// Мировая точка -> локальное пространство бокса (масштаб компонента учитывается
	// InverseTransformPosition). Margin задан в мировых см; при масштабе актора != 1
	// приближённо делим на масштаб по осям (для зоны деревни масштаб ожидается 1).
	const FTransform& BoxTM = ZoneBox->GetComponentTransform();
	const FVector Local = BoxTM.InverseTransformPosition(Point);
	const FVector Extent = ZoneBox->GetUnscaledBoxExtent();

	const FVector Scale = BoxTM.GetScale3D().GetAbs();
	const FVector LocalMargin(
		Scale.X > KINDA_SMALL_NUMBER ? Margin / Scale.X : Margin,
		Scale.Y > KINDA_SMALL_NUMBER ? Margin / Scale.Y : Margin,
		Scale.Z > KINDA_SMALL_NUMBER ? Margin / Scale.Z : Margin);

	return FMath::Abs(Local.X) <= Extent.X + LocalMargin.X
		&& FMath::Abs(Local.Y) <= Extent.Y + LocalMargin.Y
		&& FMath::Abs(Local.Z) <= Extent.Z + LocalMargin.Z;
}

bool AVillageZone::IsPointInVillage(const UWorld* World, const FVector& Point, float Margin)
{
	for (const TWeakObjectPtr<AVillageZone>& ZonePtr : ActiveZones)
	{
		const AVillageZone* Zone = ZonePtr.Get();
		if (!Zone || (World && Zone->GetWorld() != World))
		{
			continue;
		}
		if (Zone->ContainsPoint(Point, Margin))
		{
			return true;
		}
	}
	return false;
}
