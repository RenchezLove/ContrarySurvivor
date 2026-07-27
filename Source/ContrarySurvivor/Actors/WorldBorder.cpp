// Fill out your copyright notice in the Description page of Project Settings.

#include "WorldBorder.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AWorldBorder::AWorldBorder()
{
	// Статичная граница — тик не нужен, вся геометрия строится в OnConstruction.
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Рамка зоны — только визуальная подсказка в редакторе (как ZoneBox у AVillageZone).
	ZoneFrame = CreateDefaultSubobject<UBoxComponent>(TEXT("ZoneFrame"));
	ZoneFrame->SetupAttachment(SceneRoot);
	ZoneFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneFrame->SetCanEverAffectNavigation(false);
	ZoneFrame->ShapeColor = FColor(255, 220, 0, 255); // жёлтый — граница мира

	WallEast = CreateWall(TEXT("WallEast"));
	WallWest = CreateWall(TEXT("WallWest"));
	WallNorth = CreateWall(TEXT("WallNorth"));
	WallSouth = CreateWall(TEXT("WallSouth"));

	FogEast = CreateFogPlane(TEXT("FogEast"));
	FogWest = CreateFogPlane(TEXT("FogWest"));
	FogNorth = CreateFogPlane(TEXT("FogNorth"));
	FogSouth = CreateFogPlane(TEXT("FogSouth"));

	FogBandEast = CreateFogPlane(TEXT("FogBandEast"));
	FogBandWest = CreateFogPlane(TEXT("FogBandWest"));
	FogBandNorth = CreateFogPlane(TEXT("FogBandNorth"));
	FogBandSouth = CreateFogPlane(TEXT("FogBandSouth"));

	// Дефолтный меш карточки тумана — движковый плейн 100х100 см (ассет движка, есть всегда;
	// это НЕ контент проекта, поэтому FObjectFinder здесь допустим — прецедент: звуки в
	// WolfCharacter/StatsComponent). Оператор может заменить меш в BP (FogPlaneMesh).
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded())
	{
		FogPlaneMesh = PlaneFinder.Object;
	}
}

UBoxComponent* AWorldBorder::CreateWall(const TCHAR* SubobjectName)
{
	UBoxComponent* Wall = CreateDefaultSubobject<UBoxComponent>(SubobjectName);
	Wall->SetupAttachment(SceneRoot);

	// Канальная схема стены: блокируем ТОЛЬКО Pawn (игрок и враги упираются), всё остальное —
	// игнор: камера (ECC_Camera) не цепляется, хитскан выстрелов (ECC_Visibility) пролетает,
	// floor-трассы по каналам тоже не задеваются. ВНИМАНИЕ: запросы ПО ТИПУ ОБЪЕКТА
	// (WorldStatic) стену видят — но вертикальные floor-трассы бьют внутри зоны, а стена
	// стоит за её границей, так что не мешает.
	Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Wall->SetCollisionObjectType(ECC_WorldStatic);
	Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
	Wall->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Wall->ShapeColor = FColor(255, 80, 80, 255); // красный каркас в редакторе (в игре скрыт)
	return Wall;
}

UStaticMeshComponent* AWorldBorder::CreateFogPlane(const TCHAR* SubobjectName)
{
	UStaticMeshComponent* Fog = CreateDefaultSubobject<UStaticMeshComponent>(SubobjectName);
	Fog->SetupAttachment(SceneRoot);

	// Туман — чисто визуальная карточка: без коллизии, без навигации, без теней (мобилка).
	Fog->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Fog->SetCanEverAffectNavigation(false);
	Fog->SetCastShadow(false);
	return Fog;
}

void AWorldBorder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildBorder();
}

void AWorldBorder::BeginPlay()
{
	Super::BeginPlay();

	// Подстраховка: геометрия соответствует параметрам и в рантайме (RebuildBorder идемпотентен).
	RebuildBorder();

	UE_LOG(LogTemp, Log, TEXT("WorldBorder '%s': zone %.0fx%.0f cm, wall h=%.0f, fog curtain h=%.0f (mat %s), fog band depth=%.0f z=%.0f (mat %s)"),
		*GetName(), ZoneSizeX, ZoneSizeY, WallHeight,
		FogHeight, FogMaterial ? TEXT("set") : TEXT("NOT set"),
		FogDepth, FogBandHeight, FogBandMaterial ? TEXT("set") : TEXT("NOT set"));
}

void AWorldBorder::AbsorbActorScaleIntoSize()
{
	// Ринат на приёмке 07-27 подгонял зону под карту гизмо Scale — интуитивно, как привык
	// с Volume. Но размеры зоны живут в ZoneSizeX/Y, и скейл актора поверх них искажал
	// толщину стен и пропорции тумана, а числа в Details переставали соответствовать
	// реальности. Поэтому ведём себя как Volume: скейл ПОГЛОЩАЕТСЯ в размеры (X/Y — в
	// размеры зоны, Z — в высоты), после чего скейл корня сбрасывается в единичный.
	// Итог: тянешь за Scale — зона растёт, толщины/пропорции не плывут, в Details всегда
	// честные сантиметры. Уже отскейленный на карте экземпляр поглотится при первом же
	// перестроении (загрузка карты в редакторе / правка свойства / перенос актора).
	//
	// Менять скейл корня ЗДЕСЬ безопасно (проверено по исходникам UE 5.5):
	// OnConstruction — ПОСЛЕДНИЙ шаг ExecuteConstruction (ActorConstruction.cpp:974),
	// после него движок трансформ корня не трогает, т.е. сброс сохраняется; а
	// USceneComponent::SetRelativeScale3D (SceneComponent.cpp:1577) лишь обновляет
	// трансформ и НЕ запускает RerunConstructionScripts — рекурсии перестроения нет.
	if (!SceneRoot)
	{
		return;
	}

	const FVector Scale = SceneRoot->GetRelativeScale3D();
	if (Scale.Equals(FVector::OneVector))
	{
		return; // штатный случай: скейл единичный, поглощать нечего (идемпотентность)
	}

	// Ось со скейлом ~0 (схлопнутый гизмо) в размер не вносим — иначе зона выродится в ноль.
	const auto SafeAxis = [](FVector::FReal AxisScale) -> float
	{
		const float AbsScale = FMath::Abs(static_cast<float>(AxisScale));
		return AbsScale > UE_KINDA_SMALL_NUMBER ? AbsScale : 1.0f;
	};

	ZoneSizeX *= SafeAxis(Scale.X);
	ZoneSizeY *= SafeAxis(Scale.Y);
	WallHeight *= SafeAxis(Scale.Z);
	FogHeight *= SafeAxis(Scale.Z);

	// FogDepth и FogBandHeight скейлом НЕ трогаем — осознанный выбор. Глубина полосы одна
	// на все четыре стороны: при неравномерном X/Y-скейле поглощение дало бы из одного
	// параметра две разные глубины — вместо этого «толщина тумана» остаётся такой, какой
	// её выставил дизайнер. Высота полосы привязана к росту персонажа (пояс), а не к
	// размеру зоны. Под новый размер зоны полосы всё равно перестроятся в RebuildBorder.

	SceneRoot->SetRelativeScale3D(FVector::OneVector);
}

void AWorldBorder::RebuildBorder()
{
	// Сначала поглотить возможный скейл актора — геометрия ниже считается в мировых см.
	AbsorbActorScaleIntoSize();

	const float HalfX = ZoneSizeX * 0.5f;
	const float HalfY = ZoneSizeY * 0.5f;
	const float HalfH = WallHeight * 0.5f;
	const float HalfT = WallThickness * 0.5f;

	if (ZoneFrame)
	{
		ZoneFrame->SetRelativeLocation(FVector(0.0f, 0.0f, HalfH));
		ZoneFrame->SetBoxExtent(FVector(HalfX, HalfY, HalfH), /*bUpdateOverlaps=*/false);
	}

	// Стены: центр вынесен наружу на полтолщины — внутренняя грань стены точно на границе
	// зоны; по длине стены выступают на толщину, чтобы углы сомкнулись без щелей.
	struct FWallDef
	{
		UBoxComponent* Wall;
		FVector Loc;
		FVector Extent;
	};
	const FWallDef Walls[] = {
		{ WallEast,  FVector(HalfX + HalfT, 0.0f, HalfH),  FVector(HalfT, HalfY + WallThickness, HalfH) },
		{ WallWest,  FVector(-HalfX - HalfT, 0.0f, HalfH), FVector(HalfT, HalfY + WallThickness, HalfH) },
		{ WallNorth, FVector(0.0f, HalfY + HalfT, HalfH),  FVector(HalfX + WallThickness, HalfT, HalfH) },
		{ WallSouth, FVector(0.0f, -HalfY - HalfT, HalfH), FVector(HalfX + WallThickness, HalfT, HalfH) },
	};
	for (const FWallDef& Def : Walls)
	{
		if (Def.Wall)
		{
			Def.Wall->SetRelativeLocation(Def.Loc);
			Def.Wall->SetBoxExtent(Def.Extent, /*bUpdateOverlaps=*/false);
		}
	}

	// Вертикальные завесы: вплотную изнутри к стенам (отступ FogInset), нормалью внутрь зоны —
	// задник против черноты за краем карты (при наклоне камеры ~55° видны вдали).
	const float FogZ = FogHeight * 0.5f;
	SetupFogPlane(FogEast, FVector(HalfX - FogInset, 0.0f, FogZ), 0.0f, ZoneSizeY);
	SetupFogPlane(FogWest, FVector(-HalfX + FogInset, 0.0f, FogZ), 180.0f, ZoneSizeY);
	SetupFogPlane(FogNorth, FVector(0.0f, HalfY - FogInset, FogZ), 90.0f, ZoneSizeX);
	SetupFogPlane(FogSouth, FVector(0.0f, -HalfY + FogInset, FogZ), -90.0f, ZoneSizeX);

	// Горизонтальные полосы — главный туман для top-down камеры (Pitch ~-55, замечание
	// Рината 07-27: вертикальный градиент «у земли плотнее» сверху не читается). Полоса
	// каждой стороны ложится от границы зоны ВНУТРЬ на FogDepth — игрок упирается в
	// невидимую стену, уже стоя в тумане. Глубина ограничена половиной зоны по своей оси,
	// чтобы на маленькой зоне встречные полосы не вылезали за центр.
	const float DepthX = FMath::Min(FogDepth, HalfX); // глубина полос восток/запад (поперёк = по X)
	const float DepthY = FMath::Min(FogDepth, HalfY); // глубина полос север/юг (поперёк = по Y)

	// УГЛЫ: каждая полоса продлена за оба угла зоны на глубину ПЕРПЕНДИКУЛЯРНЫХ полос
	// (длина стороны + 2×глубина): у угла соседние полосы перекрываются квадратом
	// глубина×глубина изнутри, а выступы закрывают диагональный взгляд камеры через угол
	// снаружи — дыр нет. Z-fighting в местах нахлёста исключён разносом высот: полосы
	// север/юг лежат на 2 см выше полос восток/запад. С top-down камеры (дистанция ~1000 см)
	// разница не видна, а копланарного мерцания нет — важно и для серого дефолт-материала
	// без прозрачности, пока FogBandMaterial не назначен.
	const float BandZEastWest = FogBandHeight;
	const float BandZNorthSouth = FogBandHeight + 2.0f;
	SetupFogBand(FogBandEast, FVector(HalfX - DepthX * 0.5f, 0.0f, BandZEastWest), 0.0f, DepthX, ZoneSizeY + 2.0f * DepthY);
	SetupFogBand(FogBandWest, FVector(-HalfX + DepthX * 0.5f, 0.0f, BandZEastWest), 180.0f, DepthX, ZoneSizeY + 2.0f * DepthY);
	SetupFogBand(FogBandNorth, FVector(0.0f, HalfY - DepthY * 0.5f, BandZNorthSouth), 90.0f, DepthY, ZoneSizeX + 2.0f * DepthX);
	SetupFogBand(FogBandSouth, FVector(0.0f, -HalfY + DepthY * 0.5f, BandZNorthSouth), -90.0f, DepthY, ZoneSizeX + 2.0f * DepthX);
}

void AWorldBorder::SetupFogPlane(UStaticMeshComponent* Fog, const FVector& RelLocation, float YawDeg, float SpanLength)
{
	if (!Fog)
	{
		return;
	}

	Fog->SetStaticMesh(FogPlaneMesh);
	Fog->SetMaterial(0, FogMaterial); // nullptr = дефолтный материал меша (серый) — расстановку видно

	Fog->SetRelativeLocation(RelLocation);
	// Pitch 90 ставит плейн вертикально: локальная ось X смотрит вверх (высота полосы),
	// локальная Y — вдоль стены; Yaw доворачивает нормаль ВНУТРЬ зоны (0=-X, 90=-Y, 180=+X, -90=+Y).
	Fog->SetRelativeRotation(FRotator(90.0f, YawDeg, 0.0f));
	// Движковый Plane = 100х100 см: масштаб X — высота полосы, Y — длина вдоль стены.
	Fog->SetRelativeScale3D(FVector(FogHeight / 100.0f, SpanLength / 100.0f, 1.0f));
}

void AWorldBorder::SetupFogBand(UStaticMeshComponent* Band, const FVector& RelLocation, float YawDeg, float DepthAcross, float SpanLength)
{
	if (!Band)
	{
		return;
	}

	Band->SetStaticMesh(FogPlaneMesh);
	Band->SetMaterial(0, FogBandMaterial); // nullptr = дефолтный материал меша (серый) — расстановку видно

	Band->SetRelativeLocation(RelLocation);
	// Плейн остаётся ЛЕЖАЧИМ (Pitch 0, нормаль вверх — лицом к top-down камере). Yaw
	// направляет локальную ось X плейна НАРУЖУ зоны (0=+X, 180=-X, 90=+Y, -90=-Y):
	// градиент материала M_FogWall2 идёт по U — то есть ПОПЕРЁК полосы: прозрачно
	// внутрь зоны, плотно к границе.
	// ОРИЕНТАЦИЯ UV движкового Plane по ассету НЕ ПРОВЕРЕНА (бинарный .uasset). Косвенное
	// подтверждение, что U идёт вдоль локальной X: на вертикальных завесах (там локальная X
	// смотрит вверх) градиент в редакторе наблюдался по высоте. Если направление окажется
	// перевёрнутым (плотно внутрь, прозрачно к краю) — страховка: параметр FlipVertical
	// материала M_FogWall2, переворачивает градиент без правки кода.
	Band->SetRelativeRotation(FRotator(0.0f, YawDeg, 0.0f));
	// Движковый Plane = 100х100 см: масштаб X — глубина полосы (поперёк), Y — длина вдоль стороны.
	Band->SetRelativeScale3D(FVector(DepthAcross / 100.0f, SpanLength / 100.0f, 1.0f));
}
