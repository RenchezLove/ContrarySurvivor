// Fill out your copyright notice in the Description page of Project Settings.


#include "AHeadArmor.h"

AHeadArmor::AHeadArmor()
{
	// ЧЕРНОВОЕ значение защиты слота головы (draft, на тюнинг — ревью game-lead/Рината).
	ArmorProtection = 5.0f;

	ItemName = FString("Head Armor");
}

void AHeadArmor::BeginPlay()
{
	Super::BeginPlay();
}