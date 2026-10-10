// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/QuestTrackerWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
#include "ContrarySurvivor/UI/QuestObjectiveText.h"
#include "ContrarySurvivor/Data/ContraryPdaSettings.h" // цвета основных и побочных заданий
#include "Components/TextBlock.h"

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

	// Прячемся без квеста и на модальных экранах (там квест виден в самом диалоге) —
	// перенос поведения Canvas DrawHUD. Именно содержимое, не сам виджет: Collapsed
	// на себе убил бы собственный тик (виджет не смог бы развернуться обратно).
	if (!Tracked || (PC && PC->IsAnyModalUIOpen()))
	{
		SetContentVisible(false);
		return;
	}
	SetContentVisible(true);

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
	TrackerText->SetText(FText::Format(bOwnHint ? DoneWithHintFormat : (bDone ? DoneFormat : TrackerFormat), Args));
	// Идущее задание красится цветом своего вида (10.10.2026): основное оранжевым, побочное синим.
	TrackerText->SetColorAndOpacity(FSlateColor(bDone
		? DoneColor : UContraryPdaSettings::Get()->GetQuestKindColor(Tracked->Kind)));
}
