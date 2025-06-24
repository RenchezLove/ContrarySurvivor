// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AArmor.h"
#include "AHeadArmor.generated.h"

UCLASS(Blueprintable)
class CONTRARYSURVIVOR_API AHeadArmor : public AArmor
{
	GENERATED_BODY()

public:
	AHeadArmor();

protected:
	virtual void BeginPlay() override;
};
