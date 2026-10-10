// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;
struct FQuest;

/**
 * Куда ведёт задание — ОДИН источник для метки-указателя в мире (AContrarySurvivorHUD::
 * DrawQuestTargetMarker) и отметок на карте КПК (UPdaScreenWidget). Правило прежнее, вынесено
 * из HUD без изменений (10.10.2026):
 *   - цели ещё не выполнены — актор цели по FQuest::MapMarkerTag (база врагов с таким
 *     QuestMarkerTag либо любой актор с такой обычной меткой);
 *   - задание готово к сдаче — тот, кому его сдавать: собеседник с меткой TurnInNpcTag,
 *     а при пустой метке — староста.
 */
namespace QuestTargets
{
	// Ведёт ли задание сейчас к тому, кто его принимает (готово к сдаче).
	CONTRARYSURVIVOR_API bool LeadsToReceiver(const FQuest& Quest);

	// Актор, к которому сейчас ведёт задание. null — цель не размещена на уровне, у задания
	// нет метки цели либо задание не идёт. Перебирает акторы мира — не звать каждый кадр.
	CONTRARYSURVIVOR_API AActor* FindTargetActor(UWorld* World, const FQuest& Quest);

	// Имя собеседника для игрока по его метке (подпись над головой: «Староста», «Охранник»).
	// Пустая метка — староста. Пустой текст — такого персонажа на уровне нет.
	CONTRARYSURVIVOR_API FText ResolveNpcName(UWorld* World, FName NpcTag);

	// Кто выдал задание и кому его сдавать — именами для игрока.
	CONTRARYSURVIVOR_API FText ResolveGiverName(UWorld* World, const FQuest& Quest);
	CONTRARYSURVIVOR_API FText ResolveReceiverName(UWorld* World, const FQuest& Quest);
}
