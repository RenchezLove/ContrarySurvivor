// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/QuestTrackerWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "ContrarySurvivor/Actors/ElderNPC.h"  // ожидание старосты после сценки про ноутбук
#include "ContrarySurvivor/Actors/TalkerNPC.h" // «взято ли задание, которым кончается сюжет»
#include "Components/TextBlock.h"

FText UQuestTrackerWidget::BuildElderWaitLine(EElderNotebookWaitPhase Phase, float RemainingSeconds,
	const FText& WaitFormat, const FText& ReadyText)
{
	switch (Phase)
	{
		case EElderNotebookWaitPhase::Waiting:
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Minutes"), FText::AsNumber(AElderNPC::GetNotebookWaitMinutesLeft(RemainingSeconds)));
			return FText::Format(WaitFormat, Args);
		}
		case EElderNotebookWaitPhase::Ready:
			return ReadyText;
		default:
			return FText::GetEmpty();
	}
}

void UQuestTrackerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!TrackerText)
	{
		return;
	}

	AContrarySurvivorPlayerController* PC = Cast<AContrarySurvivorPlayerController>(GetOwningPlayer());
	APlayerCharacter* Player = PC ? Cast<APlayerCharacter>(PC->GetPawn()) : nullptr;
	UQuestComponent* Quests = Player ? Player->GetQuests() : nullptr;
	const FQuest* Tracked = Quests ? Quests->GetTrackedQuest() : nullptr;

	// Строка ожидания старосты (10.10.2026): живёт в этом же блоке — отдельно, когда активного
	// задания нет, и второй строкой под ним, когда есть.
	EElderNotebookWaitPhase WaitPhase = EElderNotebookWaitPhase::None;
	FText WaitLine;
	if (Player && Quests)
	{
		const float Remaining = Player->GetElderNotebookWaitRemaining();
		WaitPhase = AElderNPC::GetNotebookWaitPhase(Player->IsElderNotebookHintShown(),
			AQuestGiverNPC::IsStoryEndingQuestTaken(Quests->GetQuests()), Remaining);
		WaitLine = BuildElderWaitLine(WaitPhase, Remaining, ElderWaitFormat, ElderWaitReadyText);
	}

	// Прячемся без квеста и на модальных экранах (там квест виден в самом диалоге) —
	// перенос поведения Canvas DrawHUD. Именно содержимое, не сам виджет: Collapsed
	// на себе убил бы собственный тик (виджет не смог бы развернуться обратно).
	if ((!Tracked && WaitLine.IsEmpty()) || (PC && PC->IsAnyModalUIOpen()))
	{
		SetContentVisible(false);
		return;
	}
	SetContentVisible(true);

	if (!Tracked)
	{
		TrackerText->SetText(WaitLine);
		TrackerText->SetColorAndOpacity(FSlateColor(WaitPhase == EElderNotebookWaitPhase::Ready ? DoneColor : ActiveColor));
		return;
	}

	// Строка целей — общий сборщик (тот же, что у реплики старосты): человеческие подписи
	// целей вместо служебных тегов, единый формат в одном месте (ADR-050, порция 2).
	FFormatNamedArguments Args;
	Args.Add(TEXT("Title"), Tracked->Title);
	Args.Add(TEXT("Objectives"),
		QuestObjectiveText::BuildObjectives(*Tracked, ObjectiveFormat, ObjectiveSeparator));

	const bool bDone = (Tracked->State == EQuestState::Completed);
	// У задания своя подсказка, куда нести готовое, — общая строка «вернись к старосте» соврала бы.
	const bool bOwnHint = bDone && !Tracked->TurnInHintText.IsEmpty();
	Args.Add(TEXT("Hint"), Tracked->TurnInHintText);
	const FText QuestLine = FText::Format(bOwnHint ? DoneWithHintFormat : (bDone ? DoneFormat : TrackerFormat), Args);
	TrackerText->SetText(WaitLine.IsEmpty() ? QuestLine
		: FText::Join(FText::FromString(TEXT("\n")), QuestLine, WaitLine));
	TrackerText->SetColorAndOpacity(FSlateColor(bDone ? DoneColor : ActiveColor));
}
