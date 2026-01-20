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

	//UE_LOG(LogTemp, Warning, TEXT("Move: X=%.2f, Y=%.2f"), MovementVector.X, MovementVector.Y);

    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn) return;

    // Получаем камеру
    APlayerCameraManager* CameraManager = GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
    FRotator CameraRot = CameraManager->GetCameraRotation();

    // Берём только рысканье (yaw) — игнорируем наклон камеры
    FRotator FlatRot(0.0f, CameraRot.Yaw, 0.0f);

    // Вычисляем экранные оси:
    // Forward — «вверх» по экрану (не путать с мировым «вперёд»)
    FVector ScreenForward = FlatRot.Vector();
    // Right — «вправо» по экрану
    FVector ScreenRight = FlatRot.RotateVector(FVector::RightVector);

    // Проекция на горизонтальную плоскость (X-Y)
    ScreenForward.Z = 0.0f;
    ScreenRight.Z = 0.0f;

    // Нормализуем (обязательно!)
    ScreenForward = ScreenForward.GetSafeNormal();
    ScreenRight = ScreenRight.GetSafeNormal();

    // Применяем движение
    ControlledPawn->AddMovementInput(ScreenForward, MovementVector.Y);  // W/S — вверх/вниз по экрану
    ControlledPawn->AddMovementInput(ScreenRight, MovementVector.X);     // A/D — влево/вправо по экрану
	/*
	DrawDebugDirectionalArrow(GetWorld(), ControlledPawn->GetActorLocation(), 
    ControlledPawn->GetActorLocation() + ScreenForward * 100.0f, 20.0f, FColor::Blue, false, 5.0f);
	DrawDebugDirectionalArrow(GetWorld(), ControlledPawn->GetActorLocation(),
    ControlledPawn->GetActorLocation() + ScreenRight * 100.0f, 20.0f, FColor::Red, false, 5.0f);
	*/

	
	/*FVector2D MovementVector = Value.Get<FVector2D>();

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
	*/
}

void AContrarySurvivorPlayerController::Interact(const FInputActionValue& Value)
{
	
}

void AContrarySurvivorPlayerController::Inventory(const FInputActionValue& Value)
{
	
}
