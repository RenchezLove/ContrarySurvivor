// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AArmor.h"
#include "APantsArmor.generated.h"

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API APantsArmor : public AArmor
{
	GENERATED_BODY()

public:
	APantsArmor();

protected:
	virtual void BeginPlay() override;
};
