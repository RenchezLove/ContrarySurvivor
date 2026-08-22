// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ContrarySurvivor/Data/ContraryQuestRow.h"

class UDataTable;
struct FQuest;

/**
 * Доступ к таблице квестов DT_Quests и сборка FQuest из строки (ADR-075 п.3, спека §б).
 * Тот же принцип мягкой деградации, что у ContraryItems: таблица не назначена
 * (UContraryDataSettings) или строка не собирается — вызывающий (староста) остаётся на
 * прежних конструкторных значениях, игра работает без таблиц.
 */
namespace ContraryQuests
{
	// Таблица квестов из настроек проекта. nullptr = не назначена.
	UDataTable* GetQuestTable();

	// Строка таблицы по имени. nullptr = таблицы/строки нет (без спама в лог).
	const FContraryQuestRow* FindRow(FName RowName);

	// Собирает ГОТОВЫЙ к выдаче FQuest из строки таблицы: QuestId = имя строки, цели/награда/
	// тексты — из строки, item-цель разрешается в СЛУЖЕБНЫЙ КЛЮЧ через таблицу предметов
	// (RequiredItemRow -> LegacyKey; сравнение рантайма не меняется — ADR-050). Состояние —
	// свежее (NotStarted, прогресс 0).
	// false = строки нет ИЛИ item-ссылка не разрешилась (тогда квест из таблицы не собирается
	// ЦЕЛИКОМ — цель, которую никто не дропает, хуже отката на прежние значения C++).
	bool BuildQuestFromRow(FName RowName, FQuest& OutQuest);
}
