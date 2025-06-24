// Fill out your copyright notice in the Description page of Project Settings.


#include "AArmor.h"

AArmor::AArmor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false; // No tick needed for Armor
    ArmorProtection = 0.0f; // Set default armor rating
}

void AArmor::BeginPlay()
{
	Super::BeginPlay();
	// You can load mesh here if you wish
}

