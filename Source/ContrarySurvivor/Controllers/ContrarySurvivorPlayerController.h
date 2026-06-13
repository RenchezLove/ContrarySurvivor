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

class UStatsComponent;

UCLASS()
class CONTRARYSURVIVOR_API AContrarySurvivorPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AContrarySurvivorPlayerController();

	// Текущая захваченная цель (для HUD/индикатора). Публичный — читается из HUD.
	UFUNCTION(BlueprintPure, Category = "Combat")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	// Каждый кадр поддерживает авто-лок на ближайшей живой цели (см. UpdateAutoTarget).
	virtual void Tick(float DeltaTime) override;

	// Радиус авто-захвата ближайшей живой цели (Unreal units). DRAFT — тюнингуется.
	// Лок по умолчанию = ближайшая живая цель в этом радиусе (развитие ADR-017).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float AutoTargetRadius = 3000.0f;

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

	// Переключение оружия (пистолет<->нож). Привязано через LEGACY ActionMapping
	// (Config/DefaultInput.ini), чтобы не плодить Enhanced Input .uasset без редактора (Фаза 3).
	UFUNCTION()
	void OnSwitchWeapon();

	// Клик/тап по экрану — захват цели под курсором (ADR-017: клик-захват).
	// Если под курсором валидный враг — захватываем (lock). Иначе текущий lock сохраняется.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void TrySelectTarget();

private:
	bool IsSprinting = false;

	// Текущая захваченная цель (держится до смерти цели или захвата новой).
	UPROPERTY()
	AActor* CurrentTarget;

	// LineTrace под курсором мыши для выбора цели
	AActor* GetActorUnderCursor();

	// Валидна ли цель для захвата/огня (существует и жива).
	bool IsValidTarget(AActor* Target) const;

	// Возвращает UStatsComponent актёра, ТОЛЬКО если это валидная цель-враг:
	// не сам игрок и несёт UStatsComponent. ТИП-АГНОСТИЧНО (бандит/волк/любой Pawn
	// со StatsComponent) — определяем «врага» по наличию компонента, не по классу.
	UStatsComponent* GetTargetStats(AActor* Actor) const;

	// Ищет ближайшую ЖИВУЮ цель (Pawn с UStatsComponent, не игрок, не мёртв)
	// в пределах AutoTargetRadius. null, если никого.
	AActor* FindNearestLivingTarget() const;

	// Авто-лок: если текущая цель невалидна (нет/умерла) — берём ближайшую живую.
	// Живой ручной lock сохраняется (ручной выбор поверх авто-ближайшего).
	void UpdateAutoTarget();
};
