// Fill out your copyright notice in the Description page of Project Settings.

#include "NavArea_Village.h"

UNavArea_Village::UNavArea_Village(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Цвет области в навигационном вьюпорте (визуальная отладка зоны деревни). Стоимость
	// прохода — дефолтная (область проходима сама по себе; исключение делает только AI-фильтр).
	DrawColor = FColor(60, 180, 75, 255); // зелёная «зелёная зона» деревни
}
