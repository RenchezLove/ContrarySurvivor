// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerStatsWidget.generated.h"

class UTextBlock;
class UProgressBar;

/**
 * Постоянная панель статов игрока на UMG (ADR-048, этап 3): HP/голод/жажда, патроны
 * экипированного дальнобоя, деньги. Раскладку WBP_PlayerStats строит Ринат по схеме
 * docs/contrary-survivor/umg-layout-guide.md. Живёт на экране всю игру (создаёт HUD
 * в BeginPlay при назначенном слоте); игрока берёт КАЖДЫЙ кадр у своего контроллера —
 * переживает респаун/смену пешки без устаревших указателей.
 */
UCLASS()
class CONTRARYSURVIVOR_API UPlayerStatsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Настройки (Class Defaults WBP_PlayerStats; владение переехало из HUD — ADR-048) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "1"))
	FString HpPrefix = TEXT("HP ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "2"))
	FString HungerPrefix = TEXT("Hunger ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "3"))
	FString ThirstPrefix = TEXT("Thirst ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "4"))
	FString AmmoPrefix = TEXT("Ammo ");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayerStats|Texts", meta = (DisplayPriority = "5"))
	FString MoneyPrefix = TEXT("Монеты ");

protected:
	// Всё обновляется каждый кадр (лёгкие SetText/SetPercent — как ежекадровый Canvas-путь).
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// --- Кубики WBP_PlayerStats (имена ТОЧНЫЕ — см. umg-layout-guide.md) ---

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;   // заполнение 0..1 ставит код

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;    // «HP 80/100»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HungerBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HungerText;    // «Hunger 64»

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ThirstBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ThirstText;    // «Thirst 71»

	// «Ammo 7 / 21  (bag 30)» — только с дальнобоем в руках, иначе прячется целиком.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AmmoText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MoneyText;     // «Монеты 150»
};
