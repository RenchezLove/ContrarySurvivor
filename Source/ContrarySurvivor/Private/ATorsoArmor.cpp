// Fill out your copyright notice in the Description page of Project Settings.


#include "ATorsoArmor.h"

ATorsoArmor::ATorsoArmor()
{
	// ЧЕРНОВОЕ значение защиты слота торса (draft, на тюнинг — ревью game-lead/Рината).
	ArmorProtection = 12.0f;

	ItemName = FString("Torso Armor");
}

void ATorsoArmor::BeginPlay()
{
	Super::BeginPlay();
}