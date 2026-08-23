// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Actors/AbandonedCar.h"
#include "ContrarySurvivor/ContrarySurvivor.h"  // LogQA (наполнение лута машины)
#include "ContrarySurvivor/Components/CorpseLootComponent.h" // контейнер обыска машины
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"        // GetScalarParameterValue (проверка параметра битости)
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

AAbandonedCar::AAbandonedCar()
{
	// Чистый визуал-конструктор: тик не нужен вовсе.
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Кузов — несущая деталь: к нему крепится всё остальное и только он несёт коллизию.
	Body = CreatePart(TEXT("Body"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Body.SM_AbandonedCar_Body"));
	if (Body)
	{
		Body->SetupAttachment(SceneRoot);
		// Блокирует всё (Pawn — не пройти; Visibility — укрытие от выстрелов и гейта прямой
		// видимости атак ИИ, фикс 08-07), КРОМЕ камеры: пружина топ-даун камеры не должна
		// дёргаться, когда машина попадает между камерой и игроком.
		Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Body->SetCollisionObjectType(ECC_WorldStatic);
		Body->SetCollisionResponseToAllChannels(ECR_Block);
		Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	DoorFL = CreatePart(TEXT("DoorFL"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Door_FL.SM_AbandonedCar_Door_FL"));
	DoorFR = CreatePart(TEXT("DoorFR"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Door_FR.SM_AbandonedCar_Door_FR"));
	DoorRL = CreatePart(TEXT("DoorRL"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Door_RL.SM_AbandonedCar_Door_RL"));
	DoorRR = CreatePart(TEXT("DoorRR"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Door_RR.SM_AbandonedCar_Door_RR"));
	Hood = CreatePart(TEXT("Hood"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Hood.SM_AbandonedCar_Hood"));
	Trunk = CreatePart(TEXT("Trunk"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Trunk.SM_AbandonedCar_Trunk"));
	Glass = CreatePart(TEXT("Glass"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Glass.SM_AbandonedCar_Glass"));
	Scratches = CreatePart(TEXT("Scratches"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Scratches.SM_AbandonedCar_Scratches"));
	WheelFL = CreatePart(TEXT("WheelFL"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Wheel.SM_AbandonedCar_Wheel"));
	WheelFR = CreatePart(TEXT("WheelFR"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Wheel.SM_AbandonedCar_Wheel"));
	WheelRL = CreatePart(TEXT("WheelRL"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Wheel.SM_AbandonedCar_Wheel"));
	WheelRR = CreatePart(TEXT("WheelRR"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Wheel.SM_AbandonedCar_Wheel"));
	Block = CreatePart(TEXT("Block"), TEXT("/Game/Environment/Props/AbandonedCar/SM_AbandonedCar_Block.SM_AbandonedCar_Block"));

	// Обыск машины (Report1 п.9): отдельное хранилище — своё окно, группу вокруг себя не
	// собирает и в чужие группы не входит. Машина после обыска, разумеется, никуда не
	// девается — общий выключатель убирания тел здесь погашен.
	CarLoot = CreateDefaultSubobject<UCorpseLootComponent>(TEXT("CarLoot"));
	CarLoot->bStandaloneStash = true;
	CarLoot->bSinkWhenSearched = false;
	CarLoot->SearchTitle = NSLOCTEXT("AbandonedCar", "SearchTitle", "Обыск машины");
	CarLoot->SearchObjectName = NSLOCTEXT("AbandonedCar", "SearchObjectName", "Машина");
}

FInt32Interval AAbandonedCar::MoneyRangeForRichness(ECarLootRichness Richness) const
{
	switch (Richness)
	{
	case ECarLootRichness::Cheap:     return CheapMoney;
	case ECarLootRichness::Expensive: return ExpensiveMoney;
	case ECarLootRichness::Medium:
	default:                          return MediumMoney;
	}
}

void AAbandonedCar::BeginPlay()
{
	Super::BeginPlay();

	// Только живая игра: в редакторе машина — визуал-конструктор, лут не нужен.
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !CarLoot || !bHasLoot)
	{
		return;
	}

	// Деньги — из диапазона дороговизны; перепутанные «от..до» не роняют, а чинятся.
	const FInt32Interval Range = MoneyRangeForRichness(LootRichness);
	const int32 Money = FMath::RandRange(FMath::Min(Range.Min, Range.Max),
		FMath::Max(Range.Min, Range.Max));

	// Предметы — общий формат списка мешка (ADR-076 п.10), спавнит общий помощник пикапа.
	TArray<AMasterInventoryItem*> Items =
		APickup::SpawnLootEntries(World, PlacedLootList, GetActorLocation(), GetName());

	// Регистрация в реестре обыскиваемых — контроллер даёт подсказку «Обыскать» и
	// открывает окно; «отдельное хранилище» удерживает машину вне групповых обысков.
	CarLoot->InitLoot(static_cast<float>(Money), Items, /*bRegisterSearchable=*/true);
	UE_LOG(LogQA, Display, TEXT("QA: CAR '%s' loot ready — money=%d (%s), items=%d"),
		*GetName(), Money,
		*StaticEnum<ECarLootRichness>()->GetNameStringByValue(static_cast<int64>(LootRichness)),
		Items.Num());
}

UStaticMeshComponent* AAbandonedCar::CreatePart(const TCHAR* SubobjectName, const TCHAR* MeshAssetPath)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(SubobjectName);
	if (!Part)
	{
		return nullptr;
	}
	// Детали крепятся к кузову (если он уже создан — кузов создаётся первым; сам кузов
	// зовёт эту фабрику до аттача и цепляется к корню в конструкторе).
	Part->SetupAttachment(Body ? static_cast<USceneComponent*>(Body) : static_cast<USceneComponent*>(SceneRoot));
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetGenerateOverlapEvents(false);

	// ⚠ FObjectFinder здесь НАМЕРЕННО НЕ static: static кэшируется по месту вызова, и общий
	// хелпер закэшировал бы ПЕРВЫЙ путь на все детали (та же ловушка описана в AArmorTiers.cpp).
	ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(MeshAssetPath);
	if (MeshFinder.Succeeded())
	{
		Part->SetStaticMesh(MeshFinder.Object);
	}
	return Part;
}

void AAbandonedCar::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 1) Наличие деталей.
	SetPartVisible(DoorFL, bDoorFL);
	SetPartVisible(DoorFR, bDoorFR);
	SetPartVisible(DoorRL, bDoorRL);
	SetPartVisible(DoorRR, bDoorRR);
	SetPartVisible(Hood, bHood);
	SetPartVisible(Trunk, bTrunk);
	SetPartVisible(WheelFL, bWheelFL);
	SetPartVisible(WheelFR, bWheelFR);
	SetPartVisible(WheelRL, bWheelRL);
	SetPartVisible(WheelRR, bWheelRR);
	SetPartVisible(Scratches, bScratches);
	SetPartVisible(Block, bBlock);

	// 2) Посадка и углы створок: монтажная точка (схема сборки паспорта) + поворот вокруг
	// петли. Двери — вокруг вертикали; капот/багажник — вокруг оси X (ось петли по паспорту).
	ApplyHinge(DoorFL, DoorFLMount, DoorFLHinge, DoorFLOpenAngleDeg, /*bYaw=*/true);
	ApplyHinge(DoorFR, DoorFRMount, DoorFRHinge, DoorFROpenAngleDeg, /*bYaw=*/true);
	ApplyHinge(DoorRL, DoorRLMount, DoorRLHinge, DoorRLOpenAngleDeg, /*bYaw=*/true);
	ApplyHinge(DoorRR, DoorRRMount, DoorRRHinge, DoorRROpenAngleDeg, /*bYaw=*/true);
	ApplyHinge(Hood, HoodMount, HoodHinge, HoodOpenAngleDeg, /*bYaw=*/false);
	ApplyHinge(Trunk, TrunkMount, TrunkHinge, TrunkOpenAngleDeg, /*bYaw=*/false);

	// 3) Цвет: перекраска кузова (+ по желанию створок) и затемнение битых деталей
	// (Report1 п.9 — галочка «битая») одним проходом: итог = база · затемнение.
	const bool bPaintParts = bOverrideBodyColor && bPaintMovableParts;
	ApplyPartColor(Body, bOverrideBodyColor, /*bBroken=*/false);
	ApplyPartColor(DoorFL, bPaintParts, bDoorFLBroken);
	ApplyPartColor(DoorFR, bPaintParts, bDoorFRBroken);
	ApplyPartColor(DoorRL, bPaintParts, bDoorRLBroken);
	ApplyPartColor(DoorRR, bPaintParts, bDoorRRBroken);
	ApplyPartColor(Hood, bPaintParts, bHoodBroken);
	ApplyPartColor(Trunk, bPaintParts, bTrunkBroken);
	// Колёса не красятся кузовным цветом; битость пробует тот же параметр цвета — у
	// материала без него (резина) визуала честно нет, остаются данные.
	ApplyPartColor(WheelFL, /*bPaintOverride=*/false, bWheelFLBroken);
	ApplyPartColor(WheelFR, /*bPaintOverride=*/false, bWheelFRBroken);
	ApplyPartColor(WheelRL, /*bPaintOverride=*/false, bWheelRLBroken);
	ApplyPartColor(WheelRR, /*bPaintOverride=*/false, bWheelRRBroken);

	// 4) Стёкла: есть/нет, целые/битые.
	ApplyGlassState();
}

void AAbandonedCar::SetPartVisible(UStaticMeshComponent* Part, bool bVisible)
{
	if (Part)
	{
		Part->SetVisibility(bVisible);
		// Скрытая деталь и в игре скрыта (SetVisibility хватает: HiddenInGame не трогаем,
		// чтобы не расходиться с превью редактора).
		Part->SetHiddenInGame(!bVisible);
	}
}

void AAbandonedCar::ApplyHinge(UStaticMeshComponent* Part, const FVector& MountLocal,
	const FVector& HingeLocal, float AngleDeg, bool bYaw)
{
	if (!Part)
	{
		return;
	}
	// Посадка + поворот вокруг точки-петли (формула по паспорту моделлера, патч 22.08):
	// позиция = монтаж + петля − поворот·петля. Точка на оси петли неподвижна при любом
	// угле; при пивоте-на-петле (штатные меши комплекта) поле петли — ноль, и при нулевом
	// угле деталь стоит ровно в монтажной точке.
	// Оси: двери — рыскание (вертикальная петля); капот/багажник — КРЕН (ось петли X по
	// паспорту: нос машины = -Y, петли идут поперёк кузова). FRotator = (тангаж, рыскание, крен).
	const FRotator Rot = bYaw ? FRotator(0.0f, AngleDeg, 0.0f) : FRotator(0.0f, 0.0f, AngleDeg);
	const FQuat Q = Rot.Quaternion();
	const FVector NewLoc = MountLocal + HingeLocal - Q.RotateVector(HingeLocal);
	Part->SetRelativeLocationAndRotation(NewLoc, Q);
}

void AAbandonedCar::ApplyPartColor(UStaticMeshComponent* Part, bool bPaintOverride, bool bBroken)
{
	if (!Part)
	{
		return;
	}
	UMaterialInterface* Current = Part->GetMaterial(0);
	if (!Current)
	{
		return;
	}

	// База: перекраска — цвет из поля; иначе — родное значение параметра цвета. Родное
	// спрашиваем у ИСХОДНОГО материала (родителя MID): в самом MID после прежних правок
	// галочек лежит уже наше значение, а не заводское.
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Current);
	UMaterialInterface* Pristine = MID ? static_cast<UMaterialInterface*>(MID->Parent) : Current;

	FLinearColor Base = BodyColor;
	if (!bPaintOverride)
	{
		FLinearColor Native = FLinearColor::White;
		if (!Pristine || !Pristine->GetVectorParameterValue(
			FHashedMaterialParameterInfo(PaintColorParamName), Native))
		{
			// Параметра цвета у материала детали нет (например, резина колеса): красить и
			// затемнять нечем. Данные битости при этом живут — визуал уточняется у Рината.
			return;
		}
		Base = Native;
	}

	FLinearColor Final = Base;
	if (bBroken)
	{
		const float Tint = FMath::Clamp(BrokenTintMultiplier, 0.0f, 1.0f);
		Final = FLinearColor(Base.R * Tint, Base.G * Tint, Base.B * Tint, Base.A);
	}

	// Нечего менять и нечего откатывать (MID не создавался) — не плодим инстансы зря.
	if (!bPaintOverride && !bBroken && !MID)
	{
		return;
	}

	if (!MID)
	{
		MID = UMaterialInstanceDynamic::Create(Current, this);
		if (!MID)
		{
			return;
		}
		Part->SetMaterial(0, MID);
		PaintMIDs.Add(MID);
	}
	MID->SetVectorParameterValue(PaintColorParamName, Final);
}

void AAbandonedCar::ApplyGlassState()
{
	if (!Glass)
	{
		return;
	}
	if (!bGlass)
	{
		SetPartVisible(Glass, false);
		return;
	}

	// Есть ли у материала стекла параметр «битости»? Спрашиваем БАЗОВЫЙ материал (не MID):
	// нам важно, предусмотрел ли параметр автор материала.
	UMaterialInterface* Current = Glass->GetMaterial(0);
	float Unused = 0.0f;
	// FHashedMaterialParameterInfo — как в Pickup.cpp (проверка параметра свечения):
	// возвращает false, если параметра с таким именем у материала нет.
	const bool bHasBrokenParam = Current
		&& Current->GetScalarParameterValue(FHashedMaterialParameterInfo(GlassBrokenParamName), Unused);

	if (bGlassBroken && !bHasBrokenParam)
	{
		// Деградация по спеке: параметра нет — «битые» честно означает «стёкол не видно».
		SetPartVisible(Glass, false);
		return;
	}

	SetPartVisible(Glass, true);
	if (bHasBrokenParam)
	{
		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Current);
		if (!MID)
		{
			MID = UMaterialInstanceDynamic::Create(Current, this);
			if (!MID)
			{
				return;
			}
			Glass->SetMaterial(0, MID);
			GlassMID = MID;
		}
		MID->SetScalarParameterValue(GlassBrokenParamName, bGlassBroken ? 1.0f : 0.0f);
	}
}
