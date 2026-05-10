// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivorPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h"
#include "ARangedWeapon.h"
#include "Engine/HitResult.h"

AContrarySurvivorPlayerController::AContrarySurvivorPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	CurrentTarget = nullptr;
}

void AContrarySurvivorPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}

	// Показываем курсор мыши (нужен для выбора цели кликом)
	bShowMouseCursor = true;
	bEnableClickEvents = true;
}

void AContrarySurvivorPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent))
	{
		// Движение
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Move);

		// Спринт
		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Sprint);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AContrarySurvivorPlayerController::Sprint);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("SprintAction is null. Check Blueprint/Header assignment."));
		}

		// Действия
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Interact);
		EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Inventory);

		// Стрельба
		if (FireAction)
		{
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Fire);
		}

		// Перезарядка
		if (ReloadAction)
		{
			EnhancedInputComponent->BindAction(ReloadAction, ETriggerEvent::Triggered, this, &AContrarySurvivorPlayerController::Reload);
		}
	}
}

void AContrarySurvivorPlayerController::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn) return;

	APlayerCameraManager* CameraManager = GetWorld()->GetFirstPlayerController()->PlayerCameraManager;
	FRotator CameraRot = CameraManager->GetCameraRotation();
	FRotator FlatRot(0.0f, CameraRot.Yaw, 0.0f);

	FVector ScreenForward = FlatRot.Vector();
	FVector ScreenRight = FlatRot.RotateVector(FVector::RightVector);

	ScreenForward.Z = 0.0f;
	ScreenRight.Z = 0.0f;
	ScreenForward = ScreenForward.GetSafeNormal();
	ScreenRight = ScreenRight.GetSafeNormal();

	ControlledPawn->AddMovementInput(ScreenForward, MovementVector.Y);
	ControlledPawn->AddMovementInput(ScreenRight, MovementVector.X);
}

void AContrarySurvivorPlayerController::Sprint(const FInputActionValue& Value)
{
	APawn* ControlledPawn = GetPawn();
	if (auto* PlayerChar = Cast<AMasterHumanoidCharacter>(ControlledPawn))
	{
		PlayerChar->SetSprint(Value.Get<bool>());
	}
}

void AContrarySurvivorPlayerController::Fire(const FInputActionValue& Value)
{
	TrySelectTarget();

	AMasterHumanoidCharacter* PlayerChar = Cast<AMasterHumanoidCharacter>(GetPawn());
	if (!PlayerChar) return;

	if (CurrentTarget)
	{
		if (ARangedWeapon* Weapon = Cast<ARangedWeapon>(PlayerChar->GetCurrentWeapon()))
		{
			Weapon->SetTarget(CurrentTarget);
		}

		PlayerChar->FireCurrentWeapon(CurrentTarget);
	}
	else
	{
		PlayerChar->FireCurrentWeapon(nullptr);
	}
}

void AContrarySurvivorPlayerController::Reload(const FInputActionValue& Value)
{
	AMasterHumanoidCharacter* PlayerChar = Cast<AMasterHumanoidCharacter>(GetPawn());
	if (PlayerChar)
	{
		PlayerChar->ReloadCurrentWeapon();
	}
}

void AContrarySurvivorPlayerController::TrySelectTarget()
{
	AActor* HitActor = GetActorUnderCursor();

	if (HitActor)
	{
		CurrentTarget = HitActor;
		UE_LOG(LogTemp, Warning, TEXT("Target selected: %s"), *CurrentTarget->GetName());
	}
	else
	{
		CurrentTarget = nullptr;
	}
}

AActor* AContrarySurvivorPlayerController::GetActorUnderCursor()
{
	FHitResult HitResult;

	// LineTrace под позицией курсора мыши
	bool bHit = GetHitResultUnderCursor(ECC_Pawn, false, HitResult);

	if (bHit && HitResult.GetActor())
	{
		return HitResult.GetActor();
	}

	return nullptr;
}

void AContrarySurvivorPlayerController::Interact(const FInputActionValue& Value)
{
	// TODO: реализовать взаимодействие с предметами и NPC
}

void AContrarySurvivorPlayerController::Inventory(const FInputActionValue& Value)
{
	// TODO: открыть/закрыть инвентарь
}
