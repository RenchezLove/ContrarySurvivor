// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivorHUD.h"
#include "Engine/Canvas.h"
#include "EngineUtils.h" // TActorIterator
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"

void AContrarySurvivorHUD::DrawHUD()
{
	Super::DrawHUD();

	UWorld* World = GetWorld();
	if (!World || !Canvas)
	{
		return;
	}

	// Залоченная цель игрока (приоритет показа). Геттер контроллера: GetCurrentTarget() -> AActor*.
	AActor* LockedTarget = nullptr;
	APlayerController* PC = GetOwningPlayerController();
	if (AContrarySurvivorPlayerController* CSPC = Cast<AContrarySurvivorPlayerController>(PC))
	{
		LockedTarget = CSPC->GetCurrentTarget();
	}

	// Точка отсчёта для дистанции — пешка игрока (если есть), иначе позиция камеры/контроллера.
	FVector PlayerLocation = FVector::ZeroVector;
	bool bHavePlayerLocation = false;
	if (PC)
	{
		if (APawn* PlayerPawn = PC->GetPawn())
		{
			PlayerLocation = PlayerPawn->GetActorLocation();
			bHavePlayerLocation = true;
		}
	}

	const float RadiusSq = HealthBarShowRadius * HealthBarShowRadius;

	for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
	{
		AEnemyCharacter* Enemy = *It;
		if (!IsValid(Enemy))
		{
			continue;
		}

		UStatsComponent* Stats = Enemy->GetStats();
		if (!Stats || Stats->IsDead())
		{
			continue; // мёртвых не показываем
		}

		// Условие показа: залочена ИЛИ в радиусе от игрока.
		const bool bIsLocked = (LockedTarget == Enemy);
		bool bInRadius = false;
		if (!bIsLocked && bHavePlayerLocation)
		{
			bInRadius = FVector::DistSquared(PlayerLocation, Enemy->GetActorLocation()) <= RadiusSq;
		}

		if (bIsLocked || bInRadius)
		{
			DrawEnemyHealthBar(Enemy);
		}
	}
}

void AContrarySurvivorHUD::DrawEnemyHealthBar(AEnemyCharacter* Enemy)
{
	if (!Enemy || !Canvas)
	{
		return;
	}

	UStatsComponent* Stats = Enemy->GetStats();
	if (!Stats)
	{
		return;
	}

	// Мировая точка над головой врага.
	const FVector WorldAnchor = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, HealthBarWorldZOffset);

	// Project: X,Y — экранные координаты, Z — глубина (>0 если перед камерой). За камерой — не рисуем.
	const FVector ScreenPos = Project(WorldAnchor, false);
	if (ScreenPos.Z <= 0.0f)
	{
		return;
	}

	// Отбрасываем то, что заведомо за пределами экрана.
	if (ScreenPos.X < -HealthBarWidth || ScreenPos.X > Canvas->SizeX + HealthBarWidth ||
		ScreenPos.Y < -HealthBarHeight || ScreenPos.Y > Canvas->SizeY + HealthBarHeight)
	{
		return;
	}

	const float HealthPercent = FMath::Clamp(Stats->GetHealthPercent(), 0.0f, 1.0f);

	// Полоску центрируем по горизонтали над якорем.
	const float BarX = ScreenPos.X - HealthBarWidth * 0.5f;
	const float BarY = ScreenPos.Y - HealthBarHeight;

	// Фон.
	DrawRect(BackgroundColor, BarX, BarY, HealthBarWidth, HealthBarHeight);

	// Заполнение по проценту здоровья.
	const float FillWidth = HealthBarWidth * HealthPercent;
	if (FillWidth > 0.0f)
	{
		DrawRect(FillColor, BarX, BarY, FillWidth, HealthBarHeight);
	}
}
