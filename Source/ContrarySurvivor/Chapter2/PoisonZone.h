// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "PoisonZone.generated.h"

class UBoxComponent;
class UBillboardComponent;
class UMaterialInterface;
class AAnomalySample;

/**
 * Место аномального образца внутри отравленной низины (правка Рината 25.10.2026 вечер:
 * «отравленную зону (с артефактами, зеленой дымкой и т д) отдельным BP, что бы я мог подвигать
 * её по карте»). Это точка-компонент чертежа низины: двигаешь низину — едет и она. При старте
 * игры на ней появляется образец с этим именем.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent, DisplayName = "Место образца"))
class CONTRARYSURVIVOR_API UAnomalySampleSpotComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Под этим именем подбор образца пишется в сохранение (Sample1, Sample2, Sample3 — как у
	// образцов, стоявших на уровне раньше: старые сохранения читаются как прежде). Пусто —
	// «Sample» и номер места по порядку.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Образец", meta = (DisplayName = "Имя образца для сохранения (латиницей)"))
	FName SampleId = NAME_None;
};

/**
 * Отравленная низина главы 2 (Ринат 23.10.2026: «На карте отравленная (химически) зона рядом с
 * базой учёных, зеленоватый туман»). Объём-коробка: внутри без дыхательного аппарата (или с
 * пустым баллоном) здоровье тает каждую секунду; у края появляется кнопка «Надеть дыхательный
 * аппарат».
 *
 * Правка 25.10 вечер: низина — ОДИН объект вместе с туманом и местами образцов. Чертёж
 * BP_PoisonZone (родитель — этот класс): корень — коробка низины; места трёх образцов —
 * компоненты SampleSpot1–SampleSpot3 (в редакторе видны значками, двигаются мышью); диски
 * тумана — обычные компоненты-плоскости с материалом тумана, их добавляют в чертёж. При старте
 * игры на каждом месте появляется образец, если образца с таким именем на уровне ещё нет.
 *
 * Числа — в настройках проекта «Contrary Survivor: глава 2». Метка AnomalyLowland добавляется
 * сама: на неё ведёт метка задания «Образцы из аномалий».
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API APoisonZone : public AActor
{
	GENERATED_BODY()

public:
	APoisonZone();

	// Внутри ли точка (с учётом поворота и масштаба коробки).
	bool IsInside(const FVector& WorldPoint) const;

	// Расстояние от точки до коробки (0 — внутри).
	float DistanceTo(const FVector& WorldPoint) const;

	UBoxComponent* GetVolume() const { return Volume; }

	// Поставить образцы на места низины (зовёт BeginPlay; публичная — её зовёт автопроверка).
	// Образец с тем же именем уже есть на уровне — место пропускается. Возвращает, сколько
	// образцов поставлено.
	int32 SpawnSamplesAtSpots();

	// Имя образца для места (поле места, иначе «Sample» и номер по порядку с единицы).
	static FName ResolveSpotSampleId(const UAnomalySampleSpotComponent* Spot, int32 Index);

	const TArray<TWeakObjectPtr<AAnomalySample>>& GetSpawnedSamples() const { return SpawnedSamples; }

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	// Границы низины: размер задаётся полем «Box Extent» коробки (половина стороны, см).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Низина")
	TObjectPtr<UBoxComponent> Volume;

	// Места трёх образцов (свои места можно добавить в чертёже компонентом «Место образца»).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Низина")
	TObjectPtr<UAnomalySampleSpotComponent> SampleSpot1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Низина")
	TObjectPtr<UAnomalySampleSpotComponent> SampleSpot2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Низина")
	TObjectPtr<UAnomalySampleSpotComponent> SampleSpot3;

	// Какой образец ставить на места (чертёж образца или сам класс).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Низина", meta = (DisplayName = "Чертёж образца"))
	TSubclassOf<AAnomalySample> SampleClass;

	// Светящийся материал образца (M_AnomalyGlow). Пусто — материал чертежа образца.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Низина", meta = (DisplayName = "Материал свечения образца"))
	TObjectPtr<UMaterialInterface> SampleGlowMaterial;

	// Ставить ли образец на землю под местом (луч вниз), а не ровно в точку места.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Низина", meta = (DisplayName = "Ставить образцы на землю"))
	bool bSnapSamplesToGround = true;

private:
	TArray<TWeakObjectPtr<AAnomalySample>> SpawnedSamples;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<TObjectPtr<UBillboardComponent>> SpotSprites;
#endif
};

/**
 * Где баллон заправляется (решение лида 23.10: «баллон заправляется при входе на базу учёных»).
 * Коробка накрывает базу учёных. Нет ни одной на уровне — баллон заправляет Вера при разговоре.
 * Она же — территория базы для наёмника: туда он за героем не идёт.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AAirRefillZone : public AActor
{
	GENERATED_BODY()

public:
	AAirRefillZone();

	bool IsInside(const FVector& WorldPoint) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Заправка")
	TObjectPtr<UBoxComponent> Volume;
};

namespace Chapter2Volumes
{
	// Внутри ли точка коробки (общая проверка обеих зон).
	bool IsInsideBox(const UBoxComponent* Box, const FVector& WorldPoint);

	// Расстояние до коробки в мировых сантиметрах (0 — внутри).
	float DistanceToBox(const UBoxComponent* Box, const FVector& WorldPoint);
}
