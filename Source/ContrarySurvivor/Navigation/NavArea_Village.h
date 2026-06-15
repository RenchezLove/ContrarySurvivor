// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NavAreas/NavArea.h"
#include "NavArea_Village.generated.h"

/**
 * Навигационная область «Деревня» (BugReport 12).
 *
 * Класс-маркер для NavModifierVolume, который unreal-operator ставит над деревней. Сам по себе
 * проходим (это обычная nav-area, не блокирующая); смысл в том, что AI-фильтр врагов
 * (UNavQueryFilter_ExcludeVillage) помечает ЭТУ область как Excluded → бандиты и волки при
 * MoveTo НЕ строят путь через деревню. Нейтралы (NPC без AEnemyAIController) фильтр не
 * используют и ходят нормально.
 *
 * UNavArea — Config=Engine, Blueprintable (NavArea.h, UE 5.5). Наследник без своих полей —
 * достаточно самого факта отдельного класса-типа для фильтра.
 */
UCLASS()
class CONTRARYSURVIVOR_API UNavArea_Village : public UNavArea
{
	GENERATED_BODY()

public:
	UNavArea_Village(const FObjectInitializer& ObjectInitializer);
};
