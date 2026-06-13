// Fill out your copyright notice in the Description page of Project Settings.


#include "APantsArmor.h"

APantsArmor::APantsArmor()
{
	// ЧЕРНОВОЕ значение защиты слота штанов (draft, на тюнинг — ревью game-lead/Рината).
	ArmorProtection = 8.0f;

	ItemName = FString("Pants Armor");
}

void APantsArmor::BeginPlay()
{
	Super::BeginPlay();
}