// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldBorder.generated.h"

class UBoxComponent;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Граница игрового мира (задача Рината, дословно): «Сейчас если игрок дойдет до края карты,
 * то он просто провалится в низ в черноту (пустоту). Надо как - то ограничить мир. Я
 * предлагаю непроходимую стену из тумана. Просто вокруг карты плотный туман, а в плотную
 * к нему - невидимая стена.»
 *
 * ИНСТРУМЕНТ-актор (образец оформления — AMasterEnemyBase): ставит и подгоняет по карте
 * Ринат сам. Актор — ЦЕНТР прямоугольной игровой зоны; размеры EditAnywhere наверху Details.
 * OnConstruction перестраивает стены/туман при правке параметров на размещённом экземпляре;
 * в редакторе видна жёлтая рамка зоны (в игре скрыта).
 *
 * ПОВЕДЕНИЕ КАК У VOLUME: актор можно тянуть гизмо Scale — масштаб ПОГЛОЩАЕТСЯ в размеры
 * зоны (см. AbsorbActorScaleIntoSize), скейл актора всегда возвращается к единичному.
 *
 * УСТРОЙСТВО:
 *  - 4 НЕВИДИМЫЕ СТЕНЫ (UBoxComponent) по периметру: блокируют ТОЛЬКО канал Pawn — игрок
 *    и враги не выйдут за край и не провалятся в пустоту. Все остальные каналы игнорируются:
 *    камера (ECC_Camera, спринг-арм) и хитскан выстрелов (ECC_Visibility, ARangedWeapon —
 *    проверено по PerformLineTrace) проходят сквозь стену. Внутренняя грань стены = граница
 *    зоны, по длине стены выступают на толщину — углы закрыты без щелей.
 *  - 4 ГОРИЗОНТАЛЬНЫЕ ПОЛОСЫ ТУМАНА (лежащие плейны) по периметру — ГЛАВНЫЙ туман для
 *    top-down камеры (Pitch ~-55; Ринат 07-27: вертикальный градиент «у земли плотнее»
 *    сверху не читается). Полоса каждой стороны ложится от границы зоны ВНУТРЬ на FogDepth,
 *    на высоте FogBandHeight (пояс персонажа — игрок у края «входит в туман» и упирается
 *    в невидимую стену уже внутри полосы). Градиент материала — поперёк полосы: прозрачно
 *    внутрь зоны, плотно к краю.
 *  - 4 ВЕРТИКАЛЬНЫЕ ЗАВЕСЫ ТУМАНА (UStaticMeshComponent, дефолтный меш — движковый Plane
 *    100х100 см из /Engine/BasicShapes) вплотную изнутри к стенам, нормалью внутрь зоны.
 *    Оставлены как ЗАДНИК: при наклоне камеры ~55° закрывают черноту за краем карты вдали.
 *    Дёшево для мобилки: 8 статик-плейнов суммарно, без частиц, тени выключены. Материалы
 *    (FogMaterial — завесы, FogBandMaterial — полосы) назначает оператор в BP, пути в C++
 *    не хардкодим. Пока материал пуст — видна серая плоскость дефолта меша, так расстановку
 *    видно сразу.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AWorldBorder : public AActor
{
	GENERATED_BODY()

public:
	AWorldBorder();

protected:
	// Перестраивает стены/туман/рамку при правке параметров на размещённом экземпляре.
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	// === НАСТРОЙКИ ЗОНЫ (наверху Details, тюнинг Рината на размещённом экземпляре) ===

	// ГЛАВНАЯ ручка тумана для вида сверху: ширина горизонтальной полосы тумана в плане, см.
	// Полоса ложится от границы зоны ВНУТРЬ на эту величину — это «толщина» тумана глазами игрока.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (ClampMin = "100.0", UIMin = "100.0", DisplayPriority = "1"))
	float FogDepth = 2500.0f;

	// Полный размер игровой зоны по X, см. Актор — центр зоны.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (ClampMin = "1000.0", UIMin = "1000.0", DisplayPriority = "2"))
	float ZoneSizeX = 30000.0f;

	// Полный размер игровой зоны по Y, см.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (ClampMin = "1000.0", UIMin = "1000.0", DisplayPriority = "3"))
	float ZoneSizeY = 30000.0f;

	// Высота размещения горизонтальной полосы над землёй (уровнем актора), см. Дефолт ~120 —
	// уровень пояса персонажа: игрок у края зоны «входит в туман», а не идёт под ним.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (ClampMin = "0.0", DisplayPriority = "4"))
	float FogBandHeight = 120.0f;

	// Материал горизонтальной полосы тумана (градиент по U — поперёк полосы). Назначит
	// оператор в BP-обёртке; пусто = серый дефолт движкового плейна (расстановка видна).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (DisplayPriority = "5"))
	UMaterialInterface* FogBandMaterial = nullptr;

	// Высота невидимых стен, см (с запасом, чтобы не перепрыгнуть/не перелететь).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (ClampMin = "100.0", DisplayPriority = "6"))
	float WallHeight = 2000.0f;

	// Высота вертикальной завесы тумана, см (задник против черноты за краем карты).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (ClampMin = "100.0", DisplayPriority = "7"))
	float FogHeight = 2500.0f;

	// Материал вертикальной завесы тумана. Ассет сделает оператор позже и назначит здесь/в
	// BP-обёртке; пусто = серый дефолт движкового плейна (расстановка видна и без материала).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (DisplayPriority = "8"))
	UMaterialInterface* FogMaterial = nullptr;

	// Меш-«карточка» полосы тумана. Дефолт — движковый Plane (100х100 см), менять не обязательно.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder|Advanced")
	UStaticMesh* FogPlaneMesh = nullptr;

	// Толщина невидимой стены, см.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder|Advanced", meta = (ClampMin = "10.0"))
	float WallThickness = 100.0f;

	// Отступ полосы тумана внутрь зоны от стены, см («вплотную к туману — невидимая стена»).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder|Advanced", meta = (ClampMin = "0.0"))
	float FogInset = 30.0f;

	// === Компоненты ===

	// Корень-трансформ (placeable; позиция актора = центр зоны).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	USceneComponent* SceneRoot;

	// Жёлтая рамка зоны во вьюпорте редактора (bHiddenInGame; коллизии/навигации нет).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UBoxComponent* ZoneFrame;

	// Невидимые стены по сторонам зоны (+X / -X / +Y / -Y). Блокируют только Pawn.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UBoxComponent* WallEast;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UBoxComponent* WallWest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UBoxComponent* WallNorth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UBoxComponent* WallSouth;

	// Вертикальные завесы тумана вдоль соответствующих стен (нормаль внутрь зоны; задник).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogEast;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogWest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogNorth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogSouth;

	// Горизонтальные полосы тумана вдоль сторон (лежат в плане; главный туман для вида сверху).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogBandEast;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogBandWest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogBandNorth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogBandSouth;

private:
	// Фабрики констрактора: стена с коллизией «блок только Pawn» / плоскость тумана без коллизии
	// (общая для вертикальных завес и горизонтальных полос: без коллизии/навигации/теней).
	UBoxComponent* CreateWall(const TCHAR* SubobjectName);
	UStaticMeshComponent* CreateFogPlane(const TCHAR* SubobjectName);

	// Пересчёт всей геометрии из параметров (позиции/размеры стен, плоскостей тумана, рамки).
	void RebuildBorder();

	// Поглощает нештатный Scale актора в честные размеры зоны и сбрасывает скейл в единичный
	// (Volume-подобное поведение; зачем и почему это безопасно — комментарий в .cpp).
	void AbsorbActorScaleIntoSize();

	// Ставит одну вертикальную завесу тумана: меш/материал + позиция, поворот нормалью внутрь, масштаб.
	void SetupFogPlane(UStaticMeshComponent* Fog, const FVector& RelLocation, float YawDeg, float SpanLength);

	// Ставит одну горизонтальную полосу тумана: лежачий плейн, локальная X — поперёк полосы
	// (наружу зоны), масштаб X = глубина полосы, Y = длина вдоль стороны.
	void SetupFogBand(UStaticMeshComponent* Band, const FVector& RelLocation, float YawDeg, float DepthAcross, float SpanLength);
};
