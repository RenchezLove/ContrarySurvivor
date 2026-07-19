// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

struct FQuest;

/**
 * Сборка строки целей квеста для показа игроку (ADR-050, порция 2).
 *
 * Вынесено в одно место, потому что реплика старосты (UDialogScreenWidget) и трекер
 * квеста (UQuestTrackerWidget) собирали её ОДИНАКОВО и по отдельности — расхождение
 * форматов было вопросом времени.
 *
 * Здесь же чинится протечка служебного тега (ADR-049): цель на убийство берёт
 * человеческую подпись KillObjectiveLabel («Перебить бандитов»), а не служебный тег
 * KillTargetTag («Bandit»), из-за которого игрок читал английское слово посреди
 * русской реплики. На тег откат идёт только если подпись не заполнена. Цель на
 * предмет берёт ItemObjectiveLabel, а не ключ RequiredItemName — тем же правилом,
 * что и метка на карте (ContrarySurvivorHUD.cpp:2305).
 */
namespace QuestObjectiveText
{
	// Строка всех целей квеста: «Перебить бандитов 1 из 3» и, если целей две, вторая
	// через Separator. ObjectiveFormat принимает подстановки {Objective}, {Done}, {Total}.
	// У квеста без целей возвращается пустой текст.
	FText BuildObjectives(const FQuest& Quest, const FText& ObjectiveFormat, const FText& Separator);
}
