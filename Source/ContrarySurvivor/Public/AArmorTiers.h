// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AArmor.h"
#include "AArmorTiers.generated.h"

// Ярусная броня Т1-Т3 (решение Рината 07-07): 9 предметов по живому паттерну
// AHeadArmor/ATorsoArmor/APantsArmor — конструктор задаёт слот, черновую долю защиты
// (Т1=0.05 / Т2=0.10 / Т3=0.16 на слот) и меш слота
// /Game/Characters/Shared/Armor/SK_Armor_T{1,2,3}_{Head,Torso,Legs}.
// Т0 (Clothing) — стартовая одежда, предметом магазина не является.
// Девять мелких классов держим в одном файле: различаются только константами конструктора.

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AHeadArmorT1 : public AArmor
{
	GENERATED_BODY()

public:
	AHeadArmorT1();
};

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API ATorsoArmorT1 : public AArmor
{
	GENERATED_BODY()

public:
	ATorsoArmorT1();
};

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API APantsArmorT1 : public AArmor
{
	GENERATED_BODY()

public:
	APantsArmorT1();
};

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AHeadArmorT2 : public AArmor
{
	GENERATED_BODY()

public:
	AHeadArmorT2();
};

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API ATorsoArmorT2 : public AArmor
{
	GENERATED_BODY()

public:
	ATorsoArmorT2();
};

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API APantsArmorT2 : public AArmor
{
	GENERATED_BODY()

public:
	APantsArmorT2();
};

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AHeadArmorT3 : public AArmor
{
	GENERATED_BODY()

public:
	AHeadArmorT3();
};

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API ATorsoArmorT3 : public AArmor
{
	GENERATED_BODY()

public:
	ATorsoArmorT3();
};

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API APantsArmorT3 : public AArmor
{
	GENERATED_BODY()

public:
	APantsArmorT3();
};
