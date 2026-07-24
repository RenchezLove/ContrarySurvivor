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
 * УСТРОЙСТВО:
 *  - 4 НЕВИДИМЫЕ СТЕНЫ (UBoxComponent) по периметру: блокируют ТОЛЬКО канал Pawn — игрок
 *    и враги не выйдут за край и не провалятся в пустоту. Все остальные каналы игнорируются:
 *    камера (ECC_Camera, спринг-арм) и хитскан выстрелов (ECC_Visibility, ARangedWeapon —
 *    проверено по PerformLineTrace) проходят сквозь стену. Внутренняя грань стены = граница
 *    зоны, по длине стены выступают на толщину — углы закрыты без щелей.
 *  - 4 ПЛОСКОСТИ «СТЕНЫ ТУМАНА» (UStaticMeshComponent, дефолтный меш — движковый Plane
 *    100х100 см из /Engine/BasicShapes) вплотную изнутри к стенам, нормалью внутрь зоны.
 *    Дёшево для мобилки: 4 статик-плейна, без частиц, тени выключены. Материал тумана —
 *    FogMaterial (EditAnywhere; ассет материала оператор сделает позже и назначит в BP,
 *    путь в C++ не хардкодим). Пока материал пуст — видна серая плоскость дефолта меша,
 *    так расстановку видно сразу.
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

	// Полный размер игровой зоны по X, см. Актор — центр зоны.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (ClampMin = "1000.0", UIMin = "1000.0", DisplayPriority = "1"))
	float ZoneSizeX = 30000.0f;

	// Полный размер игровой зоны по Y, см.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (ClampMin = "1000.0", UIMin = "1000.0", DisplayPriority = "2"))
	float ZoneSizeY = 30000.0f;

	// Высота невидимых стен, см (с запасом, чтобы не перепрыгнуть/не перелететь).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (ClampMin = "100.0", DisplayPriority = "3"))
	float WallHeight = 2000.0f;

	// Высота видимой полосы тумана, см.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (ClampMin = "100.0", DisplayPriority = "4"))
	float FogHeight = 2500.0f;

	// Материал стены тумана. Ассет сделает оператор позже и назначит здесь/в BP-обёртке;
	// пусто = серый дефолт движкового плейна (расстановка видна и без материала).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WorldBorder", meta = (DisplayPriority = "5"))
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

	// Плоскости стены тумана вдоль соответствующих стен (нормаль внутрь зоны).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogEast;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogWest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogNorth;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "WorldBorder")
	UStaticMeshComponent* FogSouth;

private:
	// Фабрики констрактора: стена с коллизией «блок только Pawn» / плоскость тумана без коллизии.
	UBoxComponent* CreateWall(const TCHAR* SubobjectName);
	UStaticMeshComponent* CreateFogPlane(const TCHAR* SubobjectName);

	// Пересчёт всей геометрии из параметров (позиции/размеры стен, плоскостей тумана, рамки).
	void RebuildBorder();

	// Ставит одну плоскость тумана: меш/материал + позиция, поворот нормалью внутрь, масштаб.
	void SetupFogPlane(UStaticMeshComponent* Fog, const FVector& RelLocation, float YawDeg, float SpanLength);
};
