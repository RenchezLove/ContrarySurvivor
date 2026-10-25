// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/PoisonZone.h"
#include "ContrarySurvivor/Chapter2/AnomalySample.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

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

	SampleClass = AAnomalySample::StaticClass();

	// Три места образцов вразнобой по низине (Ринат двигает их мышью в чертеже или на уровне).
	// Коробка стоит серединой на высоте 300 над землёй — места опущены к земле.
	const FVector SpotOffsets[] = { FVector(-900.0f, 700.0f, -300.0f), FVector(800.0f, 500.0f, -300.0f), FVector(-100.0f, -900.0f, -300.0f) };
	TObjectPtr<UAnomalySampleSpotComponent>* Spots[] = { &SampleSpot1, &SampleSpot2, &SampleSpot3 };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UAnomalySampleSpotComponent* Spot = CreateDefaultSubobject<UAnomalySampleSpotComponent>(
			*FString::Printf(TEXT("SampleSpot%d"), Index + 1));
		Spot->SetupAttachment(Volume);
		Spot->SetRelativeLocation(SpotOffsets[Index]);
		Spot->SampleId = FName(*FString::Printf(TEXT("Sample%d"), Index + 1));
		*Spots[Index] = Spot;
	}

#if WITH_EDITORONLY_DATA
	// Значки мест образцов в редакторе (в игре их нет). Как у движковых AInfo: компонент
	// создаётся всегда (набор подобъектов не зависит от режима запуска), картинка грузится
	// только не в служебной программе.
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UBillboardComponent* Sprite = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(
			*FString::Printf(TEXT("SampleSpot%dSprite"), Index + 1));
		if (!Sprite)
		{
			continue;
		}
		Sprite->SetupAttachment(*Spots[Index]);
		Sprite->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
		Sprite->bIsScreenSizeScaled = true;
		if (!IsRunningCommandlet())
		{
			static ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpotTexture(TEXT("/Engine/EditorResources/S_Emitter"));
			Sprite->Sprite = SpotTexture.Get();
		}
		SpotSprites.Add(Sprite);
	}
#endif
}

void APoisonZone::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// На эту метку ведёт метка задания «Образцы из аномалий».
	Tags.AddUnique(Chapter2Ids::LowlandMarkerTag);
}

void APoisonZone::BeginPlay()
{
	Super::BeginPlay();
	SpawnSamplesAtSpots();
}

FName APoisonZone::ResolveSpotSampleId(const UAnomalySampleSpotComponent* Spot, int32 Index)
{
	if (Spot && !Spot->SampleId.IsNone())
	{
		return Spot->SampleId;
	}
	return FName(*FString::Printf(TEXT("Sample%d"), Index + 1));
}

int32 APoisonZone::SpawnSamplesAtSpots()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	TArray<UAnomalySampleSpotComponent*> Spots;
	GetComponents<UAnomalySampleSpotComponent>(Spots);
	// Порядок мест — по имени компонента (SampleSpot1, SampleSpot2…): номер для пустого имени
	// образца не зависит от порядка создания компонентов в чертеже.
	Spots.Sort([](const UAnomalySampleSpotComponent& A, const UAnomalySampleSpotComponent& B)
		{ return A.GetName() < B.GetName(); });

	// Образцы, которые уже стоят на уровне (старая расстановка или повторный вызов): второй
	// образец с тем же именем считал бы подбор дважды.
	TSet<FName> Existing;
	for (TActorIterator<AAnomalySample> It(World); It; ++It)
	{
		Existing.Add(It->GetSampleId());
	}

	UClass* Class = SampleClass ? SampleClass.Get() : AAnomalySample::StaticClass();
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Spots.Num(); ++Index)
	{
		const FName Id = ResolveSpotSampleId(Spots[Index], Index);
		if (Existing.Contains(Id))
		{
			continue;
		}

		FVector Location = Spots[Index]->GetComponentLocation();
		if (bSnapSamplesToGround)
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(PoisonZoneSampleGround), /*bTraceComplex=*/false, this);
			if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.0f, 0.0f, 600.0f), Location - FVector(0.0f, 0.0f, 3000.0f),
				ECC_Visibility, Params))
			{
				Location = Hit.ImpactPoint;
			}
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bDeferConstruction = true;
		AAnomalySample* Sample = World->SpawnActor<AAnomalySample>(Class, FTransform(FRotator::ZeroRotator, Location), SpawnParams);
		if (!Sample)
		{
			continue;
		}
		Sample->InitSample(Id, SampleGlowMaterial);
		Sample->FinishSpawning(FTransform(FRotator::ZeroRotator, Location));
		SpawnedSamples.Add(Sample);
		Existing.Add(Id);
		++Spawned;
	}
	if (Spawned > 0)
	{
		UE_LOG(LogQA, Display, TEXT("QA: chapter2 низина %s поставила образцов: %d"), *GetName(), Spawned);
	}
	return Spawned;
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
