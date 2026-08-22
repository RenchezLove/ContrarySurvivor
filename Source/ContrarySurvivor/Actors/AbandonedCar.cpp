// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Actors/AbandonedCar.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
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

	// 2) Углы створок (двери — вокруг вертикали, капот/багажник — тангаж).
	ApplyHinge(DoorFL, DoorFLHinge, DoorFLOpenAngleDeg, /*bYaw=*/true);
	ApplyHinge(DoorFR, DoorFRHinge, DoorFROpenAngleDeg, /*bYaw=*/true);
	ApplyHinge(DoorRL, DoorRLHinge, DoorRLOpenAngleDeg, /*bYaw=*/true);
	ApplyHinge(DoorRR, DoorRRHinge, DoorRROpenAngleDeg, /*bYaw=*/true);
	ApplyHinge(Hood, HoodHinge, HoodOpenAngleDeg, /*bYaw=*/false);
	ApplyHinge(Trunk, TrunkHinge, TrunkOpenAngleDeg, /*bYaw=*/false);

	// 3) Цвет кузова (+ по желанию створок — они из того же материала краски).
	if (bOverrideBodyColor)
	{
		PaintPart(Body);
		if (bPaintMovableParts)
		{
			PaintPart(DoorFL);
			PaintPart(DoorFR);
			PaintPart(DoorRL);
			PaintPart(DoorRR);
			PaintPart(Hood);
			PaintPart(Trunk);
		}
	}

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

void AAbandonedCar::ApplyHinge(UStaticMeshComponent* Part, const FVector& HingeLocal, float AngleDeg, bool bYaw)
{
	if (!Part)
	{
		return;
	}
	// Поворот вокруг точки-петли: сначала возвращаем деталь в ноль, затем крутим вокруг
	// HingeLocal. Формула: новая позиция = петля - R * петля (точка петли остаётся на месте).
	// Ноль-угол = деталь ровно в авторском положении (identity) — закрыто.
	const FRotator Rot = bYaw ? FRotator(0.0f, AngleDeg, 0.0f) : FRotator(AngleDeg, 0.0f, 0.0f);
	const FQuat Q = Rot.Quaternion();
	const FVector NewLoc = HingeLocal - Q.RotateVector(HingeLocal);
	Part->SetRelativeLocationAndRotation(NewLoc, Q);
}

void AAbandonedCar::PaintPart(UStaticMeshComponent* Part)
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
	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Current);
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
	MID->SetVectorParameterValue(PaintColorParamName, BodyColor);
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
