// Fill out your copyright notice in the Description page of Project Settings.


#include "APantsArmor.h"

APantsArmor::APantsArmor()
{
	// ЧЕРНОВАЯ доля снижения урона слотом штанов [0..1] (draft Рината: процентная броня).
	ArmorProtection = 0.15f;
	ArmorSlot = EArmorSlot::Legs;

	ItemName = FString("Pants Armor");
}

void APantsArmor::BeginPlay()
{
	Super::BeginPlay();
}