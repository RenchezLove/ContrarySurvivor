// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AArmor.h"
#include "SaveTestArmorItem.generated.h"

/**
 * Тестовый двойник BP_ArmorBase — ОДИН конкретный класс брони без собственных значений.
 *
 * В проекте все девять броней (три тира x три слота) собраны из одной BP_ArmorBase, а слот,
 * защита и меш экипировки приходят СТРОКОЙ таблицы DT_Items (ADR-075). Класс сам по себе
 * несёт только дефолты AArmor: слот «торс» и защита 0. Именно на этом сломалось «Продолжить»
 * 24.08 — восстановление поднимало класс и не накладывало строку, поэтому надетые торс и
 * штаны возвращались пустой заготовкой и оба садились в слот торса.
 *
 * Наследники C++ (AHeadArmorT1 и прочие тиры) для этой проверки не годятся: они задают слот
 * и защиту в конструкторе и потому «чинят» баг сами. Нужен именно класс-пустышка, общий на
 * несколько разных предметов.
 *
 * Используется исключительно тестами (Source/ContrarySurvivor/Tests).
 */
UCLASS()
class CONTRARYSURVIVOR_API ASaveTestArmorItem : public AArmor
{
	GENERATED_BODY()
};
