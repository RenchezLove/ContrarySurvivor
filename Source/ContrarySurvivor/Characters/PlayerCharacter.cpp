// Fill out your copyright notice in the Description page of Project Settings.


// PlayerCharacter.cpp
// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Controller.h" // Enhanced Input

APlayerCharacter::APlayerCharacter()
{
    

    // Create Spring Arm Component
    SpringArmComponent = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArmComponent->SetupAttachment(RootComponent); // Attaching to RootComponent (CapsuleComponent)
    SpringArmComponent->TargetArmLength = 1500.0f;      // Distance to Character
    SpringArmComponent->SetRelativeRotation(FRotator(-70.f, 90.f, 0.f)); //Sets isometric view

    SpringArmComponent->bDoCollisionTest = false;
    //Disable collision chek for springarm. If true -> When spring arm is overlaped by something -> Camera movese closer to player

    // Create Camera Component
    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName); // Attach to   SpringArm
    CameraComponent->bUsePawnControlRotation = false;                                      // Camera not rotates whith character

    
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    //Sets that camera is not rotates whith controller

    //Parameters initialasation
    Hunger = 100.0f;
    Thirst = 100.0f;

   
    SetUpMovement();
}

void APlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Warning, TEXT("Compiler is working correctly"));
}


void APlayerCharacter::SetUpMovement()
{
    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = false; // Character moves in the direction of input...
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 180.0f, 0.0f); // ...at this rotation rate
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
}