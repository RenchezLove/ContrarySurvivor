// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableNPCInterface.generated.h"

/**
 * Интерфейс «интерактивного NPC» (Фаза 4: торговец; Фаза 5: староста и т.п.).
 *
 * Назначение — находимость: HUD (AContrarySurvivorHUD) рисует над каждым таким NPC
 * заметный маркер, а если NPC за пределами экрана — стрелку-подсказку направления по
 * краю экрана. Так игрок понимает, где искать торговца/квестового NPC.
 *
 * Реализуется в C++ (нативный интерфейс): актёр наследует IInteractableNPCInterface,
 * HUD отбирает таких актёров через AActor::Implements<UInteractableNPCInterface>().
 */
UINTERFACE(MinimalAPI)
class UInteractableNPCInterface : public UInterface
{
	GENERATED_BODY()
};

class IInteractableNPCInterface
{
	GENERATED_BODY()

public:
	// Короткая подпись для HUD-маркера (например, "Trader" / "Торговец").
	virtual FString GetNPCMarkerLabel() const = 0;

	// На сколько единиц над ActorLocation поднимать якорь маркера (над головой NPC).
	virtual float GetNPCMarkerZOffset() const { return 240.0f; }
};
