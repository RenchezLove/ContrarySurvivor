// Fill out your copyright notice in the Description page of Project Settings.

#include "WolfAIController.h"
#include "ContrarySurvivor/Characters/WolfCharacter.h"

AWolfAIController::AWolfAIController()
{
	// --- ЧЕРНОВЫЕ боевые параметры волка (draft, на ревью game-lead/Рината) ---
	// Базовые поля объявлены protected в AEnemyAIController — доступны в наследнике.
	// Урон укуса. 08-09, живой осмотр Рината: «урон по игроку от волков чуть ниже, а то он
	// слишком быстро умирает от них» — 12 -> 10. Игрок без брони держал девять укусов, теперь
	// держит десять. Заодно волк перестал кусать больнее бандита: 10 — это ровно базовый урон
	// врага (AEnemyAIController::AttackDamage), а зубы слабее ножа и дубины по смыслу.
	AttackDamage   = 10.0f;
	AttackRange    = 70.0f;   // короткая дальность атаки (поверхность-к-поверхности, см)
	AttackCooldown = 1.0f;    // период между укусами (draft)

	// Перцепция/преследование — наследуем разумные дефолты бандита; чуть ближе порог приёмки.
	DetectionRange       = 1800.0f; // волк агрессивнее/зорче (draft)
	MoveAcceptanceRadius = 50.0f;   // < AttackRange, чтобы доставал укусом

	// D6: огнестрел — только у бандитов. Волк дерётся исключительно укусом.
	bRangedAttacker = false;
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
