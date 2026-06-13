// Fill out your copyright notice in the Description page of Project Settings.


#include "AHeadArmor.h"

AHeadArmor::AHeadArmor()
{
	// ЧЕРНОВАЯ доля снижения урона слотом головы [0..1] (draft Рината: процентная броня).
	ArmorProtection = 0.10f;

	ItemName = FString("Head Armor");
}

void AHeadArmor::BeginPlay()
{
	Super::BeginPlay();
}