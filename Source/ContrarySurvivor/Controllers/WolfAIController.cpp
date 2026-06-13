// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfAIController.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"

AWolfAIController::AWolfAIController()
{
	// --- ЧЕРНОВЫЕ боевые параметры волка (draft, на ревью game-lead/Рината) ---
	// Базовые поля объявлены protected в AEnemyAIController — доступны в наследнике.
	AttackDamage   = 12.0f;   // урон укуса (draft, GDD §7.1)
	AttackRange    = 70.0f;   // короткая дальность атаки (поверхность-к-поверхности, см)
	AttackCooldown = 1.0f;    // период между укусами (draft)

	// Перцепция/преследование — наследуем разумные дефолты бандита; чуть ближе порог приёмки.
	DetectionRange       = 1800.0f; // волк агрессивнее/зорче (draft)
	MoveAcceptanceRadius = 50.0f;   // < AttackRange, чтобы доставал укусом
}

bool AWolfAIController::PerformAttack(APawn* Player)
{
	const bool bAttacked = Super::PerformAttack(Player);
	if (bAttacked)
	{
		if (AWolfCharacter* Wolf = Cast<AWolfCharacter>(GetPawn()))
		{
			Wolf->PlayBiteAnimation();
		}
	}
	return bAttacked;
}
