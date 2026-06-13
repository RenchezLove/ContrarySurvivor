// Fill out your copyright notice in the Description page of Project Settings.


#include "ATorsoArmor.h"

ATorsoArmor::ATorsoArmor()
{
	// ЧЕРНОВАЯ доля снижения урона слотом торса [0..1] (draft Рината: процентная броня).
	ArmorProtection = 0.25f;
	ArmorSlot = EArmorSlot::Torso;

	ItemName = FString("Torso Armor");
}

void ATorsoArmor::BeginPlay()
{
	Super::BeginPlay();
}