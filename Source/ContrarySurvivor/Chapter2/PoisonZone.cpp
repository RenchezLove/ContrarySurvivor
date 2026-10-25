// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/PoisonZone.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "Components/BoxComponent.h"

namespace Chapter2Volumes
{
	bool IsInsideBox(const UBoxComponent* Box, const FVector& WorldPoint)
	{
		return Box && DistanceToBox(Box, WorldPoint) <= 0.0f;
	}

	float DistanceToBox(const UBoxComponent* Box, const FVector& WorldPoint)
	{
		if (!Box)
		{
			return TNumericLimits<float>::Max();
		}
		// В своих осях коробка — ровный прямоугольник: ближайшая точка — зажим по размерам.
		const FTransform& Xform = Box->GetComponentTransform();
		const FVector Local = Xform.InverseTransformPosition(WorldPoint);
		const FVector Extent = Box->GetUnscaledBoxExtent();
		const FVector Clamped(
			FMath::Clamp(Local.X, -Extent.X, Extent.X),
			FMath::Clamp(Local.Y, -Extent.Y, Extent.Y),
			FMath::Clamp(Local.Z, -Extent.Z, Extent.Z));
		if (Clamped.Equals(Local, KINDA_SMALL_NUMBER))
		{
			return 0.0f;
		}
		return FVector::Dist(Xform.TransformPosition(Clamped), WorldPoint);
	}
}

namespace
{
	UBoxComponent* MakeVolume(AActor* Owner, const FVector& Extent)
	{
		UBoxComponent* Box = Owner->CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
		Box->InitBoxExtent(Extent);
		// Проверка «внутри ли герой» идёт расчётом, а не столкновениями: коробка ни с чем не
		// сталкивается и никому не мешает.
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetGenerateOverlapEvents(false);
		Box->SetHiddenInGame(true);
		return Box;
	}
}

APoisonZone::APoisonZone()
{
	PrimaryActorTick.bCanEverTick = false;
	Volume = MakeVolume(this, FVector(2000.0f, 2000.0f, 600.0f));
	Volume->ShapeColor = FColor(80, 200, 60);
	RootComponent = Volume;
}

void APoisonZone::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// На эту метку ведёт метка задания «Образцы из аномалий».
	Tags.AddUnique(Chapter2Ids::LowlandMarkerTag);
}

bool APoisonZone::IsInside(const FVector& WorldPoint) const
{
	return Chapter2Volumes::IsInsideBox(Volume, WorldPoint);
}

float APoisonZone::DistanceTo(const FVector& WorldPoint) const
{
	return Chapter2Volumes::DistanceToBox(Volume, WorldPoint);
}

AAirRefillZone::AAirRefillZone()
{
	PrimaryActorTick.bCanEverTick = false;
	Volume = MakeVolume(this, FVector(1500.0f, 1500.0f, 600.0f));
	Volume->ShapeColor = FColor(80, 160, 255);
	RootComponent = Volume;
}

bool AAirRefillZone::IsInside(const FVector& WorldPoint) const
{
	return Chapter2Volumes::IsInsideBox(Volume, WorldPoint);
}
