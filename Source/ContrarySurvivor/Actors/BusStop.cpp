// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Actors/BusStop.h"
#include "ContrarySurvivor/Actors/Campfire.h"
#include "ContrarySurvivor/ContrarySurvivor.h"  // LogQA (наполнение лута остановки)
#include "ContrarySurvivor/Components/CorpseLootComponent.h" // контейнер обыска остановки
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"        // GetVectorParameterValue (есть ли параметр цвета)
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/PackageName.h"                   // DoesPackageExist (ассета остановки может ещё не быть)
#include "UObject/ConstructorHelpers.h"

ABusStop::ABusStop()
{
	// Чистый визуал-конструктор: тик не нужен вовсе.
	PrimaryActorTick.bCanEverTick = false;

	// Корень неподвижный: к подвижному корню неподвижные меши крепить нельзя, а тени
	// остановки запекаются.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetMobility(EComponentMobility::Static);
	SetRootComponent(SceneRoot);

	// Навес — несущая деталь: знак и урна крепятся к нему. Навес и урна — преграда, знак — нет.
	Shelter = CreatePart(TEXT("Shelter"), TEXT("/Game/Environment/Props/BusStop/SM_BusStop"), SceneRoot, /*bBlocking=*/true);
	Sign = CreatePart(TEXT("Sign"), TEXT("/Game/Environment/Props/BusStop/SM_BusStopSign"), Shelter, /*bBlocking=*/false);
	Urn = CreatePart(TEXT("Urn"), TEXT("/Game/Environment/Props/BusStop/SM_BusStopUrn"), Shelter, /*bBlocking=*/true);

	// Точка костра. Класс дочернего актора здесь НЕ задаётся — его ставит и снимает галочка
	// «Костёр напротив остановки» (ApplyCampfire). Сама точка неподвижная (крепится к
	// неподвижному корню); костёр на ней живёт со своей подвижностью.
	CampfireChild = CreateDefaultSubobject<UChildActorComponent>(TEXT("CampfireChild"));
	CampfireChild->SetMobility(EComponentMobility::Static);
	CampfireChild->SetupAttachment(SceneRoot);
	CampfireChild->SetRelativeLocation(CampfireOffset);

	// Класс костра по умолчанию — готовый BP_Campfire (мягко: чертежа нет — поле останется
	// пустым, костёр просто не появится). Образец — PickupBP в PlayerCharacter.cpp.
	static ConstructorHelpers::FClassFinder<ACampfire> CampfireBP(TEXT("/Game/Environment/Props/Campfire/BP_Campfire"));
	if (CampfireBP.Succeeded())
	{
		CampfireClass = CampfireBP.Class;
	}

	// Обыск остановки: отдельное хранилище — своё окно, группу вокруг себя не собирает и в
	// чужие группы не входит. Остановка после обыска никуда не девается.
	StopLoot = CreateDefaultSubobject<UCorpseLootComponent>(TEXT("StopLoot"));
	StopLoot->bStandaloneStash = true;
	StopLoot->bSinkWhenSearched = false;
	StopLoot->SearchTitle = NSLOCTEXT("BusStop", "SearchTitle", "Обыск остановки");
	StopLoot->SearchObjectName = NSLOCTEXT("BusStop", "SearchObjectName", "Остановка");
}

UStaticMeshComponent* ABusStop::CreatePart(const TCHAR* SubobjectName, const TCHAR* MeshPackagePath,
	USceneComponent* Parent, bool bBlocking)
{
	UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(SubobjectName);
	if (!Part)
	{
		return nullptr;
	}
	Part->SetMobility(EComponentMobility::Static);
	Part->SetupAttachment(Parent);
	Part->SetGenerateOverlapEvents(false);
	if (bBlocking)
	{
		// Как кузов машины: блокирует всё (Pawn — не пройти; Visibility — укрытие от выстрелов
		// и гейта прямой видимости атак ИИ), КРОМЕ камеры — пружина топ-даун камеры не должна
		// дёргаться, когда навес попадает между камерой и игроком.
		Part->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Part->SetCollisionObjectType(ECC_WorldStatic);
		Part->SetCollisionResponseToAllChannels(ECR_Block);
		Part->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}
	else
	{
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Ассеты остановки делает моделлер; пока пакета нет на диске, искатель не зовём вовсе —
	// он пишет в журнал ОШИБКУ на каждый ненайденный путь. Появится пакет — меш подхватится.
	// ⚠ FObjectFinder НАМЕРЕННО НЕ static: общий помощник закэшировал бы первый путь на все
	// детали (та же ловушка описана в AbandonedCar.cpp).
	if (FPackageName::DoesPackageExist(FString(MeshPackagePath)))
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(MeshPackagePath);
		if (MeshFinder.Succeeded())
		{
			Part->SetStaticMesh(MeshFinder.Object);
		}
	}
	return Part;
}

ACampfire* ABusStop::GetCampfire() const
{
	return CampfireChild ? Cast<ACampfire>(CampfireChild->GetChildActor()) : nullptr;
}

FQuat ABusStop::ComputeSignTilt(float TiltDeg, float DirectionDeg)
{
	// Верх знака (ось Z) заваливается в сторону D = (cos, sin, 0): поворот вокруг
	// горизонтальной оси Z x D, перпендикулярной направлению завала.
	const float DirRad = FMath::DegreesToRadians(DirectionDeg);
	const FVector Axis(-FMath::Sin(DirRad), FMath::Cos(DirRad), 0.0f);
	return FQuat(Axis, FMath::DegreesToRadians(TiltDeg));
}

void ABusStop::SetRelativeIfChanged(USceneComponent* Component, const FVector& Location, const FQuat& Rotation)
{
	if (!Component)
	{
		return;
	}
	if (Component->GetRelativeLocation().Equals(Location, 0.01f)
		&& Component->GetRelativeRotation().Quaternion().Equals(Rotation, 1.e-4f))
	{
		return;
	}
	Component->SetRelativeLocationAndRotation(Location, Rotation);
}

void ABusStop::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 1) Знак (с наклоном) и урна — на свои места относительно навеса.
	SetRelativeIfChanged(Sign, SignLocation, ComputeSignTilt(SignTiltDeg, SignTiltDirectionDeg));
	SetRelativeIfChanged(Urn, UrnLocation, FQuat::Identity);

	// 2) Цвета: все три меша на одном материале, каждому пишем все три параметра.
	ApplyColors(Shelter);
	ApplyColors(Sign);
	ApplyColors(Urn);

	// 3) Костёр.
	ApplyCampfire();
}

void ABusStop::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	ApplyCampfireSavePointFlag();
}

void ABusStop::ApplyColors(UStaticMeshComponent* Part)
{
	if (!Part)
	{
		return;
	}
	const int32 NumMaterials = Part->GetNumMaterials();
	for (int32 Slot = 0; Slot < NumMaterials; ++Slot)
	{
		UMaterialInterface* Current = Part->GetMaterial(Slot);
		if (!Current)
		{
			continue;
		}

		UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Current);
		if (!MID)
		{
			// Материал без единого параметра цвета остановки не трогаем — инстансы зря не плодим.
			FLinearColor Unused;
			const bool bHasAnyParam =
				Current->GetVectorParameterValue(FHashedMaterialParameterInfo(RoofColorParamName), Unused)
				|| Current->GetVectorParameterValue(FHashedMaterialParameterInfo(WallColorParamName), Unused)
				|| Current->GetVectorParameterValue(FHashedMaterialParameterInfo(UrnColorParamName), Unused);
			if (!bHasAnyParam)
			{
				continue;
			}
			MID = UMaterialInstanceDynamic::Create(Current, this);
			if (!MID)
			{
				continue;
			}
			Part->SetMaterial(Slot, MID);
			ColorMIDs.Add(MID);
		}
		MID->SetVectorParameterValue(RoofColorParamName, RoofColor);
		MID->SetVectorParameterValue(WallColorParamName, WallColor);
		MID->SetVectorParameterValue(UrnColorParamName, UrnColor);
	}
}

void ABusStop::ApplyCampfire()
{
	if (!CampfireChild)
	{
		return;
	}

	// Сначала точка, потом костёр: дочерний актор создаётся в положении своего компонента.
	SetRelativeIfChanged(CampfireChild, CampfireOffset, FQuat::Identity);

	UClass* WantedClass = bHasCampfire ? CampfireClass.Get() : nullptr;
	if (CampfireChild->GetChildActorClass() != WantedClass)
	{
		// Смена класса сама убирает прежний костёр и создаёт новый (пустой класс — только убирает).
		CampfireChild->SetChildActorClass(WantedClass);
	}

	ApplyCampfireSavePointFlag();
}

void ABusStop::ApplyCampfireSavePointFlag()
{
	if (ACampfire* Campfire = GetCampfire())
	{
		Campfire->SetIsSavePoint(bCampfireIsSavePoint);
	}
}

void ABusStop::BeginPlay()
{
	Super::BeginPlay();

	// Только живая игра: в редакторе остановка — визуал-конструктор, лут не нужен.
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld() || !StopLoot || !bHasLoot)
	{
		return;
	}

	// Перепутанные «от..до» не роняют, а чинятся.
	const int32 Money = FMath::RandRange(FMath::Min(MoneyRange.Min, MoneyRange.Max),
		FMath::Max(MoneyRange.Min, MoneyRange.Max));

	// Предметы — общий формат списка мешка (ADR-076 п.10), спавнит общий помощник пикапа.
	TArray<AMasterInventoryItem*> Items =
		APickup::SpawnLootEntries(World, PlacedLootList, GetActorLocation(), GetName());

	// Регистрация в реестре обыскиваемых — контроллер даёт подсказку «Обыскать» и открывает
	// окно так же, как у машины.
	StopLoot->InitLoot(static_cast<float>(Money), Items, /*bRegisterSearchable=*/true);
	UE_LOG(LogQA, Display, TEXT("QA: BUSSTOP '%s' loot ready — money=%d, items=%d"),
		*GetName(), Money, Items.Num());
}
