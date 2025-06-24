// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "AArmor.generated.h"

UCLASS(Abstract, Blueprintable)
class CONTRARYSURVIVOR_API AArmor : public AActor
{
	GENERATED_BODY()

public:
	AArmor();

protected:
	virtual void BeginPlay() override;

public:
    // Mesh to represent armor when it's on the ground
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Armor", meta = (AllowPrivateAccess = "true"))
    USkeletalMesh* ArmorMesh_World;

    // Mesh to represent armor whet is's equiped
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Armor", meta = (AllowPrivateAccess = "true"))
    USkeletalMesh* ArmorMesh_Equipped;

    // Geting mesh which equipted
    UFUNCTION(BlueprintPure, Category = "Armor")
    USkeletalMesh* GetMesh() const { return ArmorMesh_Equipped; }

    // Armor parameters
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Armor", meta = (AllowPrivateAccess = "true"))
    float ArmorProtection; 
};
