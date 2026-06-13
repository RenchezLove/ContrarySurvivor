// Fill out your copyright notice in the Description page of Project Settings.

#include "QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "GameFramework/Actor.h"

UQuestComponent::UQuestComponent()
{
	// Журнал тикать не нужно — реактивен (события offer/accept/kill/turn-in).
	PrimaryComponentTick.bCanEverTick = false;
}

FQuest* UQuestComponent::FindQuestMutable(FName QuestId)
{
	for (FQuest& Q : Quests)
	{
		if (Q.QuestId == QuestId)
		{
			return &Q;
		}
	}
	return nullptr;
}

const FQuest* UQuestComponent::FindQuest(FName QuestId) const
{
	for (const FQuest& Q : Quests)
	{
		if (Q.QuestId == QuestId)
		{
			return &Q;
		}
	}
	return nullptr;
}

const FQuest* UQuestComponent::GetTrackedQuest() const
{
	for (const FQuest& Q : Quests)
	{
		if (Q.State == EQuestState::Active || Q.State == EQuestState::Completed)
		{
			return &Q;
		}
	}
	return nullptr;
}

void UQuestComponent::OfferQuest(const FQuest& Quest)
{
	if (Quest.QuestId.IsNone())
	{
		return;
	}

	// Идемпотентно: если квест уже в журнале (в любом состоянии) — не дублируем и не сбрасываем.
	if (FindQuest(Quest.QuestId) != nullptr)
	{
		return;
	}

	FQuest Added = Quest;
	Added.Progress = 0;
	Added.State = EQuestState::NotStarted;
	Quests.Add(Added);

	UE_LOG(LogQA, Display, TEXT("QA: quest OFFERED (%s, kill %d %s)"),
		*Added.QuestId.ToString(), Added.TargetCount, *Added.KillTargetTag.ToString());

	OnQuestChanged.Broadcast(Added);
}

bool UQuestComponent::AcceptQuest(FName QuestId)
{
	FQuest* Q = FindQuestMutable(QuestId);
	if (!Q || Q->State != EQuestState::NotStarted)
	{
		return false;
	}

	Q->State = EQuestState::Active;

	UE_LOG(LogQA, Display, TEXT("QA: quest ACCEPTED (kill %d %s)"),
		Q->TargetCount, *Q->KillTargetTag.ToString());

	OnQuestChanged.Broadcast(*Q);
	return true;
}

void UQuestComponent::NotifyKill(FName TargetTag)
{
	for (FQuest& Q : Quests)
	{
		if (Q.Type != EQuestType::Kill || Q.State != EQuestState::Active)
		{
			continue;
		}
		// Пустой KillTargetTag = любая цель; иначе должен совпадать тег.
		if (!Q.KillTargetTag.IsNone() && Q.KillTargetTag != TargetTag)
		{
			continue;
		}

		Q.Progress = FMath::Min(Q.Progress + 1, Q.TargetCount);
		UE_LOG(LogQA, Display, TEXT("QA: quest progress %d/%d"), Q.Progress, Q.TargetCount);

		if (Q.Progress >= Q.TargetCount)
		{
			Q.State = EQuestState::Completed;
			UE_LOG(LogQA, Display, TEXT("QA: quest COMPLETED"));
		}

		OnQuestChanged.Broadcast(Q);
	}
}

bool UQuestComponent::TurnInQuest(FName QuestId)
{
	FQuest* Q = FindQuestMutable(QuestId);
	if (!Q || Q->State != EQuestState::Completed)
	{
		return false;
	}

	Q->State = EQuestState::TurnedIn;

	// Награда деньгами начисляется в UStatsComponent владельца (игрока).
	float Balance = -1.0f;
	if (AActor* OwnerActor = GetOwner())
	{
		if (UStatsComponent* Stats = OwnerActor->FindComponentByClass<UStatsComponent>())
		{
			Stats->AddMoney(Q->RewardMoney);
			Balance = Stats->GetMoney();
		}
	}

	UE_LOG(LogQA, Display, TEXT("QA: quest TURNED IN, +%.0f money, balance %.0f"),
		Q->RewardMoney, Balance);

	OnQuestChanged.Broadcast(*Q);
	return true;
}
