// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Chapter2/ToxicAnomalyZone.h"
#include "ContrarySurvivor/Chapter2/AnomalyFormation.h"
#include "ContrarySurvivor/Chapter2/Chapter2Rules.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "Components/BillboardComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Math/RandomStream.h"
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

// ===========================================================================
// Область тумана
// ===========================================================================

namespace
{
	// Метка слоёв тумана: по ней находятся и убираются слои, пересобираемые кодом.
	const FName FogLayerTag(TEXT("PoisonFogLayer"));

	// Плоскость движка /Engine/BasicShapes/Plane — 100 на 100 см.
	constexpr float PlaneMeshSize = 100.0f;
}

UPoisonFogAreaComponent::UPoisonFogAreaComponent()
{
	InitBoxExtent(AreaHalfSize);
	ShapeColor = FColor(150, 230, 90);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetHiddenInGame(true);
	SetCanEverAffectNavigation(false);
	bAutoActivate = true;
}

float UPoisonFogAreaComponent::LayerOpacity(float InDensity, int32 InLayerCount)
{
	// Сквозь N слоёв с прозрачностью a видно (1 - a)^N фона: плотность D в середине —
	// это 1 - (1 - a)^N, отсюда a = 1 - (1 - D)^(1/N).
	const float D = FMath::Clamp(InDensity, 0.0f, 0.99f);
	const int32 N = FMath::Max(1, InLayerCount);
	return 1.0f - FMath::Pow(1.0f - D, 1.0f / static_cast<float>(N));
}

void UPoisonFogAreaComponent::OnRegister()
{
	BoxExtent = AreaHalfSize.ComponentMax(FVector(1.0f));
	Super::OnRegister();
	RebuildLayers();
}

void UPoisonFogAreaComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// Область удалили кнопкой — её слои уходят с ней (при удалении всей низины движок убирает
	// все компоненты сам).
	if (!bDestroyingHierarchy)
	{
		for (UStaticMeshComponent* Layer : Layers)
		{
			if (IsValid(Layer))
			{
				Layer->DestroyComponent();
			}
		}
		Layers.Reset();
	}
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

#if WITH_EDITOR
void UPoisonFogAreaComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	BoxExtent = AreaHalfSize.ComponentMax(FVector(1.0f));
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (IsRegistered())
	{
		RebuildLayers();
	}
}
#endif

void UPoisonFogAreaComponent::RebuildLayers()
{
	// Прежние слои — прочь (и те, что остались от копии низины: их узнаём по метке).
	for (UStaticMeshComponent* Layer : Layers)
	{
		if (IsValid(Layer))
		{
			Layer->DestroyComponent();
		}
	}
	Layers.Reset();
	TArray<USceneComponent*> Children;
	GetChildrenComponents(/*bIncludeAllDescendants=*/false, Children);
	for (USceneComponent* Child : Children)
	{
		if (IsValid(Child) && Child->ComponentHasTag(FogLayerTag))
		{
			Child->DestroyComponent();
		}
	}

	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	const AToxicAnomalyZone* Zone = Cast<AToxicAnomalyZone>(Owner);
	if (!Zone || !World || IsTemplate() || Zone->IsTemplate() || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UMaterialInterface* Material = Zone->GetFogMaterial();
	UStaticMesh* Mesh = Zone->GetFogLayerMesh();
	if (!Material || !Mesh)
	{
		UE_LOG(LogQA, Warning, TEXT("QA: chapter2 низина %s: нет материала тумана или плоскости — область %s без тумана"),
			*Zone->GetName(), *GetName());
		return;
	}

	const int32 Count = FMath::Clamp(LayerCount, 1, 6);
	const float Opacity = LayerOpacity(Density, Count);
	const FVector Half = AreaHalfSize.ComponentMax(FVector(1.0f));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Слои ровно по высоте области, снизу вверх; верхние чуть меньше нижних.
		const float Up = (Count == 1) ? 0.0f : static_cast<float>(Index) / static_cast<float>(Count - 1);
		const float Z = -Half.Z + 2.0f * Half.Z * (static_cast<float>(Index) + 0.5f) / static_cast<float>(Count);
		const float Shrink = FMath::Lerp(1.0f, FMath::Clamp(TopLayerScale, 0.3f, 1.0f), Up);

		UStaticMeshComponent* Layer = NewObject<UStaticMeshComponent>(Owner, NAME_None,
			RF_Transient | RF_TextExportTransient | RF_DuplicateTransient);
		Layer->ComponentTags.Add(FogLayerTag);
		Layer->SetStaticMesh(Mesh);
		Layer->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Layer->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Layer->SetGenerateOverlapEvents(false);
		Layer->SetCastShadow(false);
		Layer->SetCanEverAffectNavigation(false);
		Layer->SetReceivesDecals(false);
		Layer->SetupAttachment(this);
		Layer->SetRelativeLocation(FVector(0.0f, 0.0f, Z));
		Layer->SetRelativeScale3D(FVector(2.0f * Half.X * Shrink / PlaneMeshSize, 2.0f * Half.Y * Shrink / PlaneMeshSize, 1.0f));

		UMaterialInstanceDynamic* Fog = UMaterialInstanceDynamic::Create(Material, Layer);
		Fog->SetVectorParameterValue(TEXT("FogColor"), FogColor);
		Fog->SetScalarParameterValue(TEXT("MaxOpacity"), Opacity);
		Fog->SetScalarParameterValue(TEXT("EdgeFalloff"), EdgeSoftness);
		Fog->SetScalarParameterValue(TEXT("DepthFadeDistance"), GroundSoftness);
		Layer->SetMaterial(0, Fog);
		Layer->RegisterComponent();
		Layers.Add(Layer);
	}
}

// ===========================================================================
// Низина
// ===========================================================================

AToxicAnomalyZone::AToxicAnomalyZone()
{
	PrimaryActorTick.bCanEverTick = false;
	Volume = MakeVolume(this, FVector(2000.0f, 2000.0f, 600.0f));
	Volume->ShapeColor = FColor(80, 200, 60);
	RootComponent = Volume;

	FormationClass = AAnomalyFormation::StaticClass();

	// Три места аномальных образований вразнобой по зоне (Ринат двигает их мышью в чертеже или на уровне).
	// Коробка стоит серединой на высоте 300 над землёй — места опущены к земле.
	const FVector SpotOffsets[] = { FVector(-900.0f, 700.0f, -300.0f), FVector(800.0f, 500.0f, -300.0f), FVector(-100.0f, -900.0f, -300.0f) };
	TObjectPtr<UAnomalyFormationSpotComponent>* Spots[] = { &FormationSpot1, &FormationSpot2, &FormationSpot3 };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UAnomalyFormationSpotComponent* Spot = CreateDefaultSubobject<UAnomalyFormationSpotComponent>(
			*FString::Printf(TEXT("SampleSpot%d"), Index + 1));
		Spot->SetupAttachment(Volume);
		Spot->SetRelativeLocation(SpotOffsets[Index]);
		Spot->FormationId = FName(*FString::Printf(TEXT("Sample%d"), Index + 1));
		*Spots[Index] = Spot;
	}

	// Первая область тумана — у земли низины (коробка низины стоит серединой на 300 над землёй),
	// во всю её ширину; цвет — нынешний зеленоватый.
	FogArea1 = CreateDefaultSubobject<UPoisonFogAreaComponent>(TEXT("FogArea1"));
	FogArea1->SetupAttachment(Volume);
	FogArea1->SetRelativeLocation(FVector(0.0f, 0.0f, -180.0f));

	// Материал тумана и плоскость слоя (оба лежат в папках, которые всегда попадают в сборку:
	// Config/DefaultGame.ini, DirectoriesToAlwaysCook).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FogMaterialFinder(TEXT("/Game/Environment/Chapter2/M_PoisonFog.M_PoisonFog"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	FogMaterial = FogMaterialFinder.Object;
	FogLayerMesh = PlaneFinder.Object;

#if WITH_EDITORONLY_DATA
	// Значки мест образований в редакторе (в игре их нет). Как у движковых AInfo: компонент
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

void AToxicAnomalyZone::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// На эту метку ведёт метка задания «Образцы из аномалий».
	Tags.AddUnique(Chapter2Ids::LowlandMarkerTag);
}

void AToxicAnomalyZone::BeginPlay()
{
	Super::BeginPlay();
	SpawnFormationsAtSpots();
}

TArray<UPoisonFogAreaComponent*> AToxicAnomalyZone::GetFogAreas() const
{
	TArray<UPoisonFogAreaComponent*> Areas;
	GetComponents<UPoisonFogAreaComponent>(Areas);
	return Areas;
}

void AToxicAnomalyZone::AddFogArea()
{
#if WITH_EDITOR
	if (IsTemplate())
	{
		return; // в самом чертеже кнопка ничего не делает: области добавляются экземпляру на уровне
	}
	Modify();

	// Новая область — копия последней, сдвинутая вбок на половину её длины (чтобы было видно,
	// что она появилась); дальше её двигают мышью и настраивают как первую.
	const TArray<UPoisonFogAreaComponent*> Areas = GetFogAreas();
	const UPoisonFogAreaComponent* Source = Areas.Num() > 0 ? Areas.Last() : FogArea1.Get();
	int32 Number = 2;
	while (FindObjectFast<UObject>(this, FName(*FString::Printf(TEXT("FogArea%d"), Number))))
	{
		++Number;
	}
	UPoisonFogAreaComponent* Area = NewObject<UPoisonFogAreaComponent>(this, FName(*FString::Printf(TEXT("FogArea%d"), Number)), RF_Transactional);
	if (Source)
	{
		Area->AreaHalfSize = Source->AreaHalfSize;
		Area->Density = Source->Density;
		Area->FogColor = Source->FogColor;
		Area->LayerCount = Source->LayerCount;
		Area->EdgeSoftness = Source->EdgeSoftness;
		Area->GroundSoftness = Source->GroundSoftness;
		Area->TopLayerScale = Source->TopLayerScale;
		Area->SetRelativeLocation(Source->GetRelativeLocation() + FVector(Source->AreaHalfSize.X, 0.0f, 0.0f));
	}
	Area->CreationMethod = EComponentCreationMethod::Instance;
	Area->SetupAttachment(Volume);
	AddInstanceComponent(Area);
	Area->RegisterComponent();
	UE_LOG(LogQA, Display, TEXT("QA: chapter2 низина %s: добавлена область тумана %s (всего %d)"),
		*GetName(), *Area->GetName(), GetFogAreas().Num());
#endif
}

void AToxicAnomalyZone::RemoveLastFogArea()
{
#if WITH_EDITOR
	// Удаляется последняя добавленная кнопкой; первая область (FogArea1) остаётся всегда.
	const TArray<UActorComponent*>& Added = GetInstanceComponents();
	for (int32 Index = Added.Num() - 1; Index >= 0; --Index)
	{
		if (UPoisonFogAreaComponent* Area = Cast<UPoisonFogAreaComponent>(Added[Index]))
		{
			Modify();
			RemoveInstanceComponent(Area);
			Area->DestroyComponent();
			UE_LOG(LogQA, Display, TEXT("QA: chapter2 низина %s: область тумана удалена (осталось %d)"),
				*GetName(), GetFogAreas().Num());
			return;
		}
	}
#endif
}

FName AToxicAnomalyZone::ResolveSpotFormationId(const UAnomalyFormationSpotComponent* Spot, int32 Index)
{
	if (Spot && !Spot->FormationId.IsNone())
	{
		return Spot->FormationId;
	}
	return FName(*FString::Printf(TEXT("Sample%d"), Index + 1));
}

void AToxicAnomalyZone::ComputeFormationPlaces(TArray<FVector>& OutLocations, TArray<FName>& OutIds) const
{
	OutLocations.Reset();
	OutIds.Reset();
	const int32 Count = GetFormationCount();

	TArray<UAnomalyFormationSpotComponent*> Spots;
	GetComponents<UAnomalyFormationSpotComponent>(Spots);
	// Порядок мест — по имени компонента (SampleSpot1, SampleSpot2…): номер для пустого имени
	// образца не зависит от порядка создания компонентов в чертеже.
	Spots.Sort([](const UAnomalyFormationSpotComponent& A, const UAnomalyFormationSpotComponent& B)
		{ return A.GetName() < B.GetName(); });
	for (int32 Index = 0; Index < Spots.Num() && OutLocations.Num() < Count; ++Index)
	{
		OutLocations.Add(Spots[Index]->GetComponentLocation());
		OutIds.Add(ResolveSpotFormationId(Spots[Index], Index));
	}
	if (OutLocations.Num() >= Count || !Volume)
	{
		return;
	}

	// Недостающие — случайные точки в коробке низины (по числу-зерну: одни и те же при каждом
	// запуске, иначе подбор в сохранении указывал бы на другие места). Из нескольких проб —
	// та, что дальше всех от уже стоящих образований.
	FRandomStream Random(RandomPlacesSeed);
	const FTransform& Xform = Volume->GetComponentTransform();
	const FVector Extent = Volume->GetUnscaledBoxExtent();
	const double MinDistSq = FMath::Square(static_cast<double>(FMath::Max(0.0f, RandomPlacesMinDistance)));
	while (OutLocations.Num() < Count)
	{
		FVector Best = Xform.GetLocation();
		double BestDistSq = -1.0;
		for (int32 Try = 0; Try < 24; ++Try)
		{
			const FVector Local(Random.FRandRange(-0.85f, 0.85f) * Extent.X, Random.FRandRange(-0.85f, 0.85f) * Extent.Y, 0.0f);
			const FVector Candidate = Xform.TransformPosition(Local);
			double Nearest = TNumericLimits<double>::Max();
			for (const FVector& Taken : OutLocations)
			{
				Nearest = FMath::Min(Nearest, FVector::DistSquared2D(Taken, Candidate));
			}
			if (Nearest > BestDistSq)
			{
				BestDistSq = Nearest;
				Best = Candidate;
			}
			if (Nearest >= MinDistSq)
			{
				break;
			}
		}
		// Имя — следующее по номеру и не занятое местом-компонентом.
		int32 Number = OutLocations.Num() + 1;
		FName Id = FName(*FString::Printf(TEXT("Sample%d"), Number));
		while (OutIds.Contains(Id))
		{
			Id = FName(*FString::Printf(TEXT("Sample%d"), ++Number));
		}
		OutLocations.Add(Best);
		OutIds.Add(Id);
	}
}

int32 AToxicAnomalyZone::SpawnFormationsAtSpots()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	TArray<FVector> Places;
	TArray<FName> Ids;
	ComputeFormationPlaces(Places, Ids);

	// Образцы, которые уже стоят на уровне (старая расстановка или повторный вызов): второй
	// образец с тем же именем считал бы подбор дважды.
	TSet<FName> Existing;
	for (TActorIterator<AAnomalyFormation> It(World); It; ++It)
	{
		Existing.Add(It->GetFormationId());
	}

	UClass* Class = FormationClass ? FormationClass.Get() : AAnomalyFormation::StaticClass();
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Places.Num(); ++Index)
	{
		const FName Id = Ids[Index];
		if (Existing.Contains(Id))
		{
			continue;
		}

		FVector Location = Places[Index];
		if (bSnapFormationsToGround)
		{
			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ToxicZoneFormationGround), /*bTraceComplex=*/false, this);
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
		AAnomalyFormation* Formation = World->SpawnActor<AAnomalyFormation>(Class, FTransform(FRotator::ZeroRotator, Location), SpawnParams);
		if (!Formation)
		{
			continue;
		}
		Formation->InitFormation(Id, FormationGlowMaterial);
		Formation->FinishSpawning(FTransform(FRotator::ZeroRotator, Location));
		SpawnedFormations.Add(Formation);
		Existing.Add(Id);
		++Spawned;
	}
	if (Spawned > 0)
	{
		UE_LOG(LogQA, Display, TEXT("QA: chapter2 низина %s поставила аномальных образований: %d (всего в низине %d)"), *GetName(), Spawned, GetFormationCount());
	}
	return Spawned;
}

bool AToxicAnomalyZone::IsInside(const FVector& WorldPoint) const
{
	return Chapter2Volumes::IsInsideBox(Volume, WorldPoint);
}

float AToxicAnomalyZone::DistanceTo(const FVector& WorldPoint) const
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
