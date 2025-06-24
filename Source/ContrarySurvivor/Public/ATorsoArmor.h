// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AArmor.h"
#include "ATorsoArmor.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API ATorsoArmor : public AArmor
{
	GENERATED_BODY()
public:
	ATorsoArmor();

protected:
	virtual void BeginPlay() override;
};
