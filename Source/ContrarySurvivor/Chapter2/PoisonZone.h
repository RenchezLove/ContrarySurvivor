// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PoisonZone.generated.h"

class UBoxComponent;

/**
 * Отравленная низина главы 2 (Ринат 23.10.2026: «На карте отравленная (химически) зона рядом с
 * базой учёных, зеленоватый туман»). Объём-коробка: внутри без дыхательного аппарата (или с
 * пустым баллоном) здоровье тает каждую секунду; у края появляется кнопка «Надеть дыхательный
 * аппарат». Сам туман — оформление уровня, ставится рядом отдельно.
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

protected:
	virtual void PostInitializeComponents() override;

	// Границы низины: размер задаётся полем «Box Extent» коробки (половина стороны, см).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Низина")
	TObjectPtr<UBoxComponent> Volume;
};

/**
 * Где баллон заправляется (решение лида 23.10: «баллон заправляется при входе на базу учёных»).
 * Коробка накрывает базу учёных. Нет ни одной на уровне — баллон заправляет Вера при разговоре.
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
