// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "AMasterInventoryItem.h"
#include "AArmor.generated.h"

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API AArmor : public AMasterInventoryItem
{
	GENERATED_BODY()

public:
	AArmor();

protected:
	virtual void BeginPlay() override;

public:
    
    // Mesh to represent armor whet is's equiped
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Armor", meta = (AllowPrivateAccess = "true"))
    USkeletalMesh* ArmorMesh_Equipped;

    // Geting mesh which equipted
    UFUNCTION(BlueprintPure, Category = "Armor")
    USkeletalMesh* GetMesh() const { return ArmorMesh_Equipped; }

    // Доля снижения урона этим предметом брони [0..1] (решение Рината: ПРОЦЕНТНАЯ броня
    // вместо flat). Напр. 0.25 = -25% урона от слота. Тюнингуется в редакторе (EditAnywhere).
    // DRAFT-значения задаются в конструкторах конкретных слотов (Head/Torso/Pants).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Armor", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
    float ArmorProtection;

    // Доля снижения урона этим слотом [0..1]. Суммируется по экипированным слотам и
    // используется при расчёте получаемого урона (GDD §7.2: «урон рассчитывается от
    // характеристик оружия и брони»). См. AMasterHumanoidCharacter::ComputeArmoredDamage.
    UFUNCTION(BlueprintPure, Category = "Armor")
    FORCEINLINE float GetArmorProtection() const { return ArmorProtection; }
};
