// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
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
    CameraComponent->SetupAttachment(SpringArmComponent, USpringArmComponent::SocketName); // Attachto   SpringArm
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

    // ------------------------------------------------------------------------
    // Enhanced Input: Добавляем Mapping Context

    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // ------------------------------------------------------------------------
    // Enhanced Input: Привязываем действия

    if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // Moving
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);

        // Actions
        EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Interact);
        EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Triggered, this, &APlayerCharacter::Inventory);
    }
}

// ----------------------------------------------------------------------------
// Методы обработки ввода
void APlayerCharacter::Move(const FInputActionValue& Value)
{
    // input is a Vector2D
    FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        // find out which way is forward
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);

        // get forward vector
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

        // get right vector
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

        // add movement
        AddMovementInput(ForwardDirection, MovementVector.Y);
        AddMovementInput(RightDirection, MovementVector.X);
    }
}

void APlayerCharacter::Interact(const FInputActionValue& Value)
{
    bool InteractValue = Value.Get<bool>();

    if (Controller != nullptr)
    {
       // interact code 
    }
}

void APlayerCharacter::Inventory(const FInputActionValue& Value)
{
    bool InventoryValue = Value.Get<bool>();

    if (Controller != nullptr)
    {
       // inventory code 
    }
}

void APlayerCharacter::SetUpMovement()
{
    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
}
