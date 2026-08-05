// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "SaveTestPlayerCharacter.generated.h"

/**
 * Тестовый наследник APlayerCharacter — ТОЛЬКО чтобы выставить свой слот сейва напрямую.
 *
 * SaveSlotName/SaveUserIndex у APlayerCharacter — protected НАМЕРЕННО (слот сейва — идентичность
 * персонажа, а не то, что можно переключить снаружи в рантайме); подкласс к protected-полям
 * базы доступ имеет (стандартный C++). Даёт Automation-тестам сейва свой изолированный слот —
 * иначе прогон тестов (SaveGame/DeleteGameInSlot) писал бы поверх РЕАЛЬНОГО слота 'ContrarySave'
 * на диске, а на машине Рината в нём живой прогресс с телефона (ADR-058).
 *
 * Используется исключительно тестами (Source/ContrarySurvivor/Tests).
 */
UCLASS()
class CONTRARYSURVIVOR_API ASaveTestPlayerCharacter : public APlayerCharacter
{
	GENERATED_BODY()

public:
	void UseTestSaveSlot(const FString& SlotName)
	{
		SaveSlotName = SlotName;
		SaveUserIndex = 0;
	}
};
