// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "NavQueryFilter_ExcludeVillage.generated.h"

/**
 * Навигационный query-фильтр врагов: ИСКЛЮЧАЕТ область деревни (UNavArea_Village) из поиска пути
 * (BugReport 12). Передаётся в AAIController::MoveToActor как FilterClass — путь врага НЕ проходит
 * через деревню (бандиты/волки её обходят). Нейтралы фильтр не используют → ходят нормально.
 *
 * Реализация: в конструкторе заполняем массив Areas одним FNavigationFilterArea с
 * AreaClass=UNavArea_Village и bIsExcluded=true (NavigationQueryFilter.h, UE 5.5).
 */
UCLASS()
class CONTRARYSURVIVOR_API UNavQueryFilter_ExcludeVillage : public UNavigationQueryFilter
{
	GENERATED_BODY()

public:
	UNavQueryFilter_ExcludeVillage(const FObjectInitializer& ObjectInitializer);
};
