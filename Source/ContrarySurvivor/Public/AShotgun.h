// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ARangedWeapon.h"
#include "AShotgun.generated.h"

/**
 * Двустволка ТОЗ-34 (поручение Рината 09.10.2026). Обычное стрелковое оружие проекта, которое
 * отличается от пистолета только значениями полей базы: стреляет дробью (несколько дробин в
 * широком конусе), держит два патрона в стволах и заряжается своим видом патрона — дробовым
 * 12 калибра. Вся механика (выстрел дробью, перезарядка, вид патрона) живёт в ARangedWeapon;
 * здесь — только черновые числа, которые правятся в чертеже-наследнике.
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AShotgun : public ARangedWeapon
{
	GENERATED_BODY()

public:
	AShotgun();
};
