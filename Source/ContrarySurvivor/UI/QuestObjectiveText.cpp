// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "ContrarySurvivor/Components/QuestComponent.h" // FQuest (только чтение полей)

namespace
{
	// Одна цель по формату {Objective} {Done} {Total}.
	FText MakeOne(const FText& Label, int32 Done, int32 Total, const FText& Format)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Objective"), Label);
		Args.Add(TEXT("Done"), FText::AsNumber(Done));
		Args.Add(TEXT("Total"), FText::AsNumber(Total));
		return FText::Format(Format, Args);
	}
}

namespace QuestObjectiveText
{
	FText BuildObjectives(const FQuest& Quest, const FText& ObjectiveFormat, const FText& Separator)
	{
		TArray<FText> Parts;

		// Цель на убийство: человеческая подпись, служебный тег — только как откат.
		if (Quest.TargetCount > 0)
		{
			const FText Label = Quest.KillObjectiveLabel.IsEmpty()
				? FText::FromString(Quest.KillTargetTag.ToString())
				: FText::FromString(Quest.KillObjectiveLabel);
			Parts.Add(MakeOne(Label, Quest.Progress, Quest.TargetCount, ObjectiveFormat));
		}

		// Цель на предмет: подпись цели, ключ предмета — только как откат.
		if (Quest.RequiredItemCount > 0)
		{
			const FText Label = Quest.ItemObjectiveLabel.IsEmpty()
				? FText::FromString(Quest.RequiredItemName)
				: FText::FromString(Quest.ItemObjectiveLabel);
			Parts.Add(MakeOne(Label, Quest.ItemProgress, Quest.RequiredItemCount, ObjectiveFormat));
		}

		if (Parts.Num() == 0)
		{
			return FText::GetEmpty();
		}
		return FText::Join(Separator, Parts);
	}
}
