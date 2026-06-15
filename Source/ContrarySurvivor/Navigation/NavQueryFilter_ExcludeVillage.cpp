// Fill out your copyright notice in the Description page of Project Settings.

#include "NavQueryFilter_ExcludeVillage.h"
#include "NavArea_Village.h"

UNavQueryFilter_ExcludeVillage::UNavQueryFilter_ExcludeVillage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Исключаем область деревни из поиска пути: FNavigationFilterArea{ AreaClass=Village, Excluded }.
	// Заполняем публичный массив Areas (NavigationQueryFilter.h:112) напрямую в дефолтах CDO —
	// фильтр кэшируется навигацией и применяется ко всем MoveToActor, которым передан как FilterClass.
	FNavigationFilterArea VillageArea;
	VillageArea.AreaClass = UNavArea_Village::StaticClass();
	VillageArea.bIsExcluded = true;
	Areas.Add(VillageArea);
}
