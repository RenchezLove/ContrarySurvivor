// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ContrarySurvivorHUD.generated.h"

class AEnemyCharacter;

/**
 * HUD первого вертикального среза (Фаза 1).
 * Рисует полоску здоровья над врагами прямо на Canvas (UMG-граф в UE 5.5 не редактируется
 * через Python, поэтому хелсбар сделан C++-отрисовкой на AHUD::DrawHUD — без ручных шагов).
 *
 * Показываем хелсбар врага (AEnemyCharacter), если он:
 *   - жив (StatsComponent не мёртв),
 *   - попадает в кадр (Project вернул точку перед камерой),
 *   - и (залочен игроком ИЛИ находится в радиусе HealthBarShowRadius от игрока) — GDD ч.8.
 *
 * Назначается как HUDClass в BP GameMode (делает unreal-operator).
 */
UCLASS()
class CONTRARYSURVIVOR_API AContrarySurvivorHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	// Радиус (в Unreal units), в пределах которого над врагом показывается хелсбар.
	// GDD ч.8: «при приближении ближе ~5 м». 5 м ≈ 500 ед, но для top-down-обзора берём с запасом.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	float HealthBarShowRadius = 1500.0f;

	// Размеры полоски здоровья в пикселях.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	float HealthBarWidth = 80.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	float HealthBarHeight = 8.0f;

	// На сколько единиц над Actor location поднимаем якорь полоски (над головой).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	float HealthBarWorldZOffset = 110.0f;

	// Цвета.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.6f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|HealthBar")
	FLinearColor FillColor = FLinearColor(0.85f, 0.1f, 0.1f, 0.9f);

private:
	// Рисует одну полоску здоровья над врагом. Возвращает true, если что-то нарисовано.
	void DrawEnemyHealthBar(AEnemyCharacter* Enemy);
};
