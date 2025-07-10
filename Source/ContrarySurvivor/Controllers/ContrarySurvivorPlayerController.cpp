// Fill out your copyright notice in the Description page of Project Settings.


#include "ContrarySurvivorPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"

AContrarySurvivorPlayerController::AContrarySurvivorPlayerController()
{
	
	PrimaryActorTick.bCanEverTick = true;
}

void AContrarySurvivorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Enhanced Input: Adding Mapping Context
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void AContrarySurvivorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Enhanced Input: Asingn Actions
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Move);

		// Actions
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Interact);
		EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Inventory);
	}
}

void AContrarySurvivorPlayerController::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

    // Getting controlling pawn
    APawn* ControlledPawn = GetPawn();

    if (ControlledPawn != nullptr) // Check if Pawn exists
    {
        // Getting movement directon
        FVector ForwardDirection = ControlledPawn->GetActorForwardVector();
        FVector RightDirection = ControlledPawn->GetActorRightVector();

        // Applayng move
		//FVector Movement = FVector(MovementVector.X, MovementVector.Y, 0.0f);
        //ControlledPawn->SetActorLocation(ControlledPawn->GetActorLocation() + Movement);

        ControlledPawn->AddMovementInput(ForwardDirection, MovementVector.Y);
        ControlledPawn->AddMovementInput(RightDirection, MovementVector.X);
    }
}

void AContrarySurvivorPlayerController::Interact(const FInputActionValue& Value)
{
	
}

void AContrarySurvivorPlayerController::Inventory(const FInputActionValue& Value)
{
	
}
