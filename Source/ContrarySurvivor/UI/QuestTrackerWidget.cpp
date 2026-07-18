// Fill out your copyright notice in the Description page of Project Settings.

#include "ContrarySurvivor/UI/QuestTrackerWidget.h"
#include "ContrarySurvivor/Characters/PlayerCharacter.h"
#include "ContrarySurvivor/Components/QuestComponent.h"
#include "ContrarySurvivor/Controllers/ContrarySurvivorPlayerController.h"
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

	// Обобщённая строка прогресса целей (kill и/или item) — формат Canvas DrawQuestTracker.
	FString ObjStr;
	if (Tracked->TargetCount > 0)
	{
		ObjStr += FString::Printf(TEXT("%s %d/%d"),
			*Tracked->KillTargetTag.ToString(), Tracked->Progress, Tracked->TargetCount);
	}
	if (Tracked->RequiredItemCount > 0)
	{
		if (!ObjStr.IsEmpty()) { ObjStr += TEXT(", "); }
		ObjStr += FString::Printf(TEXT("%s %d/%d"),
			*Tracked->RequiredItemName, Tracked->ItemProgress, Tracked->RequiredItemCount);
	}

	const bool bDone = (Tracked->State == EQuestState::Completed);
	const FString Text = bDone
		? FString::Printf(TEXT("%s%s (%s)%s"), *DonePrefix, *Tracked->Title, *ObjStr, *DoneSuffix)
		: FString::Printf(TEXT("%s%s — %s"), *TrackerPrefix, *Tracked->Title, *ObjStr);

	TrackerText->SetText(FText::FromString(Text));
	TrackerText->SetColorAndOpacity(FSlateColor(bDone ? DoneColor : ActiveColor));
}
