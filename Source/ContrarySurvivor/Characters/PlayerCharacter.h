// Fill out your copyright notice in the Description page of Project Settings.

// PlayerCharacter.h
// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MasterHumanoidCharacter.h" // InheritingFrom от AMasterHumanoidCharacter
#include "GameFramework/SpringArmComponent.h" 
#include "Camera/CameraComponent.h" 
#include "GameFramework/Controller.h" // Enhanced Input
//#include "PlayerController.h"
#include "AMasterWeapon.h"
#include "PlayerCharacter.generated.h"

/**
 * 
 */
UCLASS()
class CONTRARYSURVIVOR_API APlayerCharacter : public AMasterHumanoidCharacter
{
    GENERATED_BODY()

public:
    APlayerCharacter();


protected:

    // Spring Arm 
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* SpringArmComponent;

    // Camera Component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* CameraComponent;

    // Stats
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float Hunger;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float Thirst;

    // Класс стартового оружия. Если задан — спавнится и экипируется в BeginPlay.
    // Значение (например, BP_Pistol) выставляется в дефолтах BP игрока.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
    TSubclassOf<AMasterWeapon> DefaultWeaponClass;

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Спавнит DefaultWeaponClass и экипирует через EquipWeapon (если класс задан).
    void EquipDefaultWeapon();

    /**
     * Sets movement parameters
     */
    void SetUpMovement();


};
