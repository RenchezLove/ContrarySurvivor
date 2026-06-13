// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnemyAIController.h"
#include "WolfAIController.generated.h"

/**
 * AI-контроллер волка (GDD §7.1, tech-design «Волк — через AEnemyCharacter»-логику).
 * Переиспользует state-machine AEnemyAIController (Idle->Chase->Attack) как есть,
 * только переопределяет ЧЕРНОВЫЕ боевые/перцептивные параметры под волка в конструкторе
 * (урон укуса, дальность, кулдаун). Меш/анимации — позже (unreal-operator).
 */
UCLASS()
class CONTRARYSURVIVOR_API AWolfAIController : public AEnemyAIController
{
	GENERATED_BODY()

public:
	AWolfAIController();

protected:
	// Расширяет атаку базы: при реальном укусе (не на кулдауне) проигрывает анимацию
	// укуса на пешке-волке (Single Node, без AnimBP).
	virtual bool PerformAttack(APawn* Player) override;
};
