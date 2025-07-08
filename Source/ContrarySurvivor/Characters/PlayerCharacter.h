// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MasterHumanoidCharacter.h" // InheritingFrom от AMasterHumanoidCharacter
#include "GameFramework/SpringArmComponent.h" 
#include "Camera/CameraComponent.h" 
#include "EnhancedInputComponent.h"  // Enhanced Input
#include "EnhancedInputSubsystems.h" // Enhanced Input
#include "InputMappingContext.h"    // Enhanced Input
#include "InputAction.h"            // Enhanced Input
#include "GameFramework/Controller.h" // Enhanced Input
#include "PlayerController.h"
#include "PlayerCharacter.generated.h"

/**
 * 
 */
UCLASS()
class CONTRARYSURVIVOR_API APlayerCharacter : public AMasterHumanoidCharacter
{
	GENERATED_BODY()
	

protected:
   

    // Spring Arm 
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    USpringArmComponent* SpringArmComponent;

    // Camera Component
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera", meta = (AllowPrivateAccess = "true"))
    UCameraComponent* CameraComponent;

    
    // Input (Enhanced Input)

    // Input Mapping Context
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    // Input Actions
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction; // Общее действие для движения

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* InteractAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* InventoryAction;

    
    // Stats
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float Hunger;

    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stats")
    float Thirst;

   

    // Movement
    void Move(const FInputActionValue& Value);

    // Actions
    void Interact(const FInputActionValue& Value);
    void Inventory(const FInputActionValue& Value);

protected:

    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // Called to bind functionality to input
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    /**
     * Sets movement parameters
     */
    void SetUpMovement();
};