// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivorHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h" // GEngine->GetMediumFont
#include "EngineUtils.h" // TActorIterator
#include "ContrarySurvivor/Characters/EnemyCharacter.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
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

	// --- Статы игрока (GDD §7.7) ---
	if (PC)
	{
		if (APlayerCharacter* PlayerChar = Cast<APlayerCharacter>(PC->GetPawn()))
		{
			DrawPlayerStats(PlayerChar->GetStats());
		}
	}
}

void AContrarySurvivorHUD::DrawPlayerStats(UStatsComponent* Stats)
{
	if (!Stats || !Canvas)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	// Компактный левый стек: HP -> Hunger -> Thirst -> Money.
	// DEV-режим (решение game-lead): голод/жажда/деньги видны ВСЕГДА, не только в
	// критической зоне. Прятать-до-критич. (GDD §7.7) вернём в финальной UX-полировке.
	const float BarX = PlayerHudMarginX;
	float CurY = PlayerHudMarginY;
	const float SurvivalMax = FMath::Max(Stats->GetSurvivalMax(), 1.0f);
	const FLinearColor MoneyColor(1.0f, 0.85f, 0.2f, 1.0f);

	// --- HP-бар (слева вверху, GDD §7.7) ---
	DrawRect(BackgroundColor, BarX, CurY, PlayerHealthBarWidth, PlayerHealthBarHeight);
	const float HpFillWidth = PlayerHealthBarWidth * FMath::Clamp(Stats->GetHealthPercent(), 0.0f, 1.0f);
	if (HpFillWidth > 0.0f)
	{
		DrawRect(PlayerHealthFillColor, BarX, CurY, HpFillWidth, PlayerHealthBarHeight);
	}
	if (Font)
	{
		DrawText(FString::Printf(TEXT("HP %.0f/%.0f"), Stats->GetHealth(), Stats->GetMaxHealth()),
			FLinearColor::White, BarX + 6.0f, CurY + 2.0f, Font);
	}
	CurY += PlayerHealthBarHeight + 6.0f;

	// Высота баров голода/жажды.
	const float SurvBarH = 16.0f;

	// --- Голод (всегда) ---
	DrawRect(BackgroundColor, BarX, CurY, PlayerHealthBarWidth, SurvBarH);
	const float HungerFillW = PlayerHealthBarWidth * FMath::Clamp(Stats->GetHunger() / SurvivalMax, 0.0f, 1.0f);
	if (HungerFillW > 0.0f)
	{
		DrawRect(HungerColor, BarX, CurY, HungerFillW, SurvBarH);
	}
	if (Font)
	{
		DrawText(FString::Printf(TEXT("Hunger %.0f"), Stats->GetHunger()),
			FLinearColor::White, BarX + 6.0f, CurY + 1.0f, Font);
	}
	CurY += SurvBarH + 4.0f;

	// --- Жажда (всегда) ---
	DrawRect(BackgroundColor, BarX, CurY, PlayerHealthBarWidth, SurvBarH);
	const float ThirstFillW = PlayerHealthBarWidth * FMath::Clamp(Stats->GetThirst() / SurvivalMax, 0.0f, 1.0f);
	if (ThirstFillW > 0.0f)
	{
		DrawRect(ThirstColor, BarX, CurY, ThirstFillW, SurvBarH);
	}
	if (Font)
	{
		DrawText(FString::Printf(TEXT("Thirst %.0f"), Stats->GetThirst()),
			FLinearColor::White, BarX + 6.0f, CurY + 1.0f, Font);
	}
	CurY += SurvBarH + 6.0f;

	// --- Деньги (всегда, читаемо в общем стеке) ---
	if (Font)
	{
		DrawText(FString::Printf(TEXT("Money %.0f"), Stats->GetMoney()),
			MoneyColor, BarX, CurY, Font);
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
