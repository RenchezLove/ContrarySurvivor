// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"                            // FTableRowBase
#include "ContrarySurvivor/Components/QuestComponent.h"  // EQuestType (+FQuest собирается из строки)
#include "ContraryQuestRow.generated.h"

/**
 * Строка таблицы квестов DT_Quests (ADR-075 п.3, спека spec-datatables-phase2.md §б).
 *
 * ИМЯ СТРОКИ (RowName) == QuestId («KillWolves», «ClearBanditBase») — латиница, стабильный
 * идентификатор журнала/сейва.
 *
 * ЧТО ЗДЕСЬ ЖИВЁТ: редактируемая часть FQuest — цели, количества, награда, тексты.
 * Рантайм-состояние (Progress/ItemProgress/State) в таблицу НЕ идёт: FQuest и UQuestComponent
 * не меняются, сейв (полные FQuest в слоте) совместим без миграции.
 *
 * СВЯЗЬ С ПРЕДМЕТАМИ — ЧЕРЕЗ СТРОКУ ТАБЛИЦЫ ПРЕДМЕТОВ (RequiredItemRow -> DT_Items), а не
 * через сырой ключ: при выдаче квеста ссылка разрешается в служебный ключ строки (LegacyKey,
 * контракт ADR-050) и кладётся в FQuest::RequiredItemName — посимвольное сравнение рантайма
 * не меняется, и связка «квест -> предмет» существует ровно в одном месте (требование
 * ADR-069/075: не плодить новую связку по ключам).
 *
 * Реплики СТАРОСТЫ (интро, намёк, FirstQuestCompletedText) — контент NPC, живут на BP_Elder
 * и в таблицу не переезжают (анти-дубль).
 */
USTRUCT(BlueprintType)
struct FContraryQuestRow : public FTableRowBase
{
	GENERATED_BODY()

	// Короткое название для журнала/диалога (переводимое).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (DisplayPriority = "1",
		DisplayName = "Название"))
	FText Title;

	// Текст задания (переводимый).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (DisplayPriority = "2",
		DisplayName = "Описание", MultiLine = "true"))
	FText Description;

	// Тип (описательный — для текста/иконки; завершённость считается по целям ниже).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (DisplayPriority = "3",
		DisplayName = "Тип"))
	EQuestType Type = EQuestType::Kill;

	// --- KILL-цель ---

	// Тег цели убийств (сверяется с полем QuestKillTag врага). None + 0 = kill-цели нет.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Kill", meta = (DisplayPriority = "4",
		DisplayName = "Тег цели убийств (латиницей)"))
	FName KillTargetTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Kill", meta = (ClampMin = "0", DisplayPriority = "5",
		DisplayName = "Сколько убить (0 = kill-цели нет)"))
	int32 TargetCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Kill", meta = (DisplayPriority = "6",
		DisplayName = "Подпись kill-цели на карте"))
	FText KillObjectiveLabel;

	// --- ITEM-цель ---

	// Строка таблицы предметов DT_Items (wolf_pelt, laptop): при выдаче квеста разрешается
	// в служебный ключ предмета. Не разрешилась — квест из таблицы НЕ собирается (откат на
	// прежние значения C++), чтобы не выдать квест с целью, которую никто не дропает.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Item", meta = (DisplayPriority = "7",
		DisplayName = "Предмет цели (строка таблицы предметов)"))
	FName RequiredItemRow = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Item", meta = (ClampMin = "0", DisplayPriority = "8",
		DisplayName = "Сколько предметов (0 = item-цели нет)"))
	int32 RequiredItemCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Item", meta = (DisplayPriority = "9",
		DisplayName = "Подпись item-цели на карте"))
	FText ItemObjectiveLabel;

	// --- Прочее ---

	// Тег актора-цели для метки на карте (QuestMarkerTag базы врагов: WolfDen, BanditBase).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (DisplayPriority = "10",
		DisplayName = "Тег метки на карте"))
	FName MapMarkerTag = NAME_None;

	// Награда деньгами при сдаче.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (ClampMin = "0.0", DisplayPriority = "11",
		DisplayName = "Награда (монеты)"))
	float RewardMoney = 0.0f;

	// Реплики героя на кнопках диалога (пусто = общие подписи панели диалога).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Реплики", meta = (DisplayPriority = "12",
		DisplayName = "Ответ героя: принять"))
	FText AcceptReplyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Реплики", meta = (DisplayPriority = "13",
		DisplayName = "Ответ героя: сдать"))
	FText TurnInReplyText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|Реплики", meta = (DisplayPriority = "14",
		DisplayName = "Ответ героя: закрыть диалог"))
	FText CloseReplyText;

	// ЗАДЕЛ под цепочки: следующая строка квеста. Сейчас НЕ потребляется — порядок «кв.1 ->
	// кв.2» ведут два слота старосты (FirstQuestRow/SecondQuestRow), длинные цепочки —
	// отдельной волной, когда появится третий квест.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest", meta = (DisplayPriority = "15",
		DisplayName = "Следующий квест (задел, не используется)"))
	FName NextQuestRow = NAME_None;
};
