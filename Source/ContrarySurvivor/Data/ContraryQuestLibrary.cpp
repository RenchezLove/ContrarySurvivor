// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/Data/ContraryQuestLibrary.h"
#include "ContrarySurvivor/Data/ContraryDataSettings.h"
#include "ContrarySurvivor/Data/ContraryItemLibrary.h"   // разрешение RequiredItemRow -> служебный ключ
#include "ContrarySurvivor/Components/QuestComponent.h"  // FQuest
#include "Engine/DataTable.h"

namespace ContraryQuests
{

UDataTable* GetQuestTable()
{
	const UContraryDataSettings* Settings = UContraryDataSettings::Get();
	if (!Settings || Settings->QuestTable.IsNull())
	{
		return nullptr;
	}
	return Settings->QuestTable.LoadSynchronous();
}

const FContraryQuestRow* FindRow(FName RowName)
{
	if (RowName.IsNone())
	{
		return nullptr;
	}
	UDataTable* Table = GetQuestTable();
	if (!Table)
	{
		return nullptr;
	}
	// bWarnIfRowMissing=false: отсутствие таблицы/строки — штатный откат, не спамим.
	return Table->FindRow<FContraryQuestRow>(RowName, TEXT("ContraryQuests::FindRow"), /*bWarnIfRowMissing=*/false);
}

bool BuildQuestFromRow(FName RowName, FQuest& OutQuest)
{
	const FContraryQuestRow* Row = FindRow(RowName);
	if (!Row)
	{
		return false;
	}

	// ITEM-цель: ссылка на строку таблицы предметов обязана разрешиться в служебный ключ —
	// иначе квест требовал бы предмет, которого не существует. Атомарно: не разрешилась —
	// квест из таблицы не собираем вовсе (вызывающий остаётся на конструкторных значениях).
	FString RequiredItemKey;
	if (Row->RequiredItemCount > 0)
	{
		const FContraryItemRow* ItemRow = ContraryItems::FindRow(Row->RequiredItemRow);
		if (!ItemRow)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("ContraryQuests::BuildQuestFromRow: у квеста '%s' предмет цели '%s' не найден в таблице предметов — квест из таблицы не собран, действуют значения из кода."),
				*RowName.ToString(), *Row->RequiredItemRow.ToString());
			return false;
		}
		RequiredItemKey = ItemRow->GetEffectiveKey(Row->RequiredItemRow);
	}

	// Свежий FQuest: состояние NotStarted, прогресс 0 (рантайм-поля таблица не задаёт).
	FQuest Quest;
	Quest.QuestId = RowName;
	Quest.Title = Row->Title;
	Quest.Description = Row->Description;
	Quest.Type = Row->Type;
	Quest.KillTargetTag = Row->KillTargetTag;
	Quest.TargetCount = Row->TargetCount;
	Quest.KillObjectiveLabel = Row->KillObjectiveLabel;
	Quest.RequiredItemName = RequiredItemKey;
	Quest.RequiredItemCount = Row->RequiredItemCount;
	Quest.ItemObjectiveLabel = Row->ItemObjectiveLabel;
	Quest.MapMarkerTag = Row->MapMarkerTag;
	Quest.RewardMoney = Row->RewardMoney;
	Quest.AcceptReplyText = Row->AcceptReplyText;
	Quest.TurnInReplyText = Row->TurnInReplyText;
	Quest.CloseReplyText = Row->CloseReplyText;

	OutQuest = Quest;
	return true;
}

} // namespace ContraryQuests
