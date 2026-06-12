// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "ContrarySurvivor/Characters/MasterHumanoidCharacter.h"
#include "ContrarySurvivorPlayerController.generated.h"

UCLASS()
class CONTRARYSURVIVOR_API AContrarySurvivorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AContrarySurvivorPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// --- Input Mapping ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	// --- Input Actions ---

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* SprintAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* InteractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* InventoryAction;

	// Клик/тап — выбор цели или выстрел
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* FireAction;

	// Перезарядка
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* ReloadAction;

	// --- Движение ---

	void Move(const FInputActionValue& Value);

	UFUNCTION()
	void Sprint(const FInputActionValue& Value);

	// --- Действия ---

	void Interact(const FInputActionValue& Value);
	void Inventory(const FInputActionValue& Value);

	// Нажатие кнопки атаки
	void Fire(const FInputActionValue& Value);

	// Перезарядка
	void Reload(const FInputActionValue& Value);

	// Клик/тап по экрану — захват цели под курсором (ADR-017: клик-захват).
	// Если под курсором валидный враг — захватываем (lock). Иначе текущий lock сохраняется.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TrySelectTarget();

	// Текущая захваченная цель (для HUD/индикатора).
	UFUNCTION(BlueprintPure, Category = "Combat")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

private:
	bool IsSprinting = false;

	// Текущая захваченная цель (держится до смерти цели или захвата новой).
	UPROPERTY()
	AActor* CurrentTarget;

	// LineTrace под курсором мыши для выбора цели
	AActor* GetActorUnderCursor();

	// Валидна ли цель для захвата/огня (существует и жива).
	bool IsValidTarget(AActor* Target) const;
};
