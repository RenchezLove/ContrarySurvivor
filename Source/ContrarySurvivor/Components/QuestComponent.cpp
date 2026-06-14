// Fill out your copyright notice in the Description page of Project Settings.

#include "QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "UInventoryComponent.h"   // изъятие предметов при сдаче / подсчёт прогресса (Фаза 5)
#include "AMasterInventoryItem.h"  // ItemName
#include "GameFramework/Actor.h"

UQuestComponent::UQuestComponent()
{
	// Журнал тикать не нужно — реактивен (события offer/accept/kill/turn-in + Sync из контроллера).
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

int32 UQuestComponent::GetTurnedInQuestCount() const
{
	int32 Count = 0;
	for (const FQuest& Q : Quests)
	{
		if (Q.State == EQuestState::TurnedIn)
		{
			++Count;
		}
	}
	return Count;
}

bool UQuestComponent::AreObjectivesMet(const FQuest& Quest)
{
	const bool bKillMet = (Quest.TargetCount <= 0) || (Quest.Progress >= Quest.TargetCount);
	const bool bItemMet = (Quest.RequiredItemCount <= 0) || (Quest.ItemProgress >= Quest.RequiredItemCount);
	return bKillMet && bItemMet;
}

bool UQuestComponent::RecomputeState(FQuest& Quest)
{
	// Пересчёт только в активной фазе (Active<->Completed). NotStarted/TurnedIn не трогаем.
	if (Quest.State != EQuestState::Active && Quest.State != EQuestState::Completed)
	{
		return false;
	}

	const EQuestState Desired = AreObjectivesMet(Quest) ? EQuestState::Completed : EQuestState::Active;
	if (Desired == Quest.State)
	{
		return false;
	}

	Quest.State = Desired;
	if (Desired == EQuestState::Completed)
	{
		UE_LOG(LogQA, Display, TEXT("QA: quest COMPLETED (%s)"), *Quest.QuestId.ToString());
	}
	else
	{
		UE_LOG(LogQA, Display, TEXT("QA: quest back to ACTIVE (%s) - objectives no longer met"), *Quest.QuestId.ToString());
	}
	return true;
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
	Added.ItemProgress = 0;
	Added.State = EQuestState::NotStarted;
	Quests.Add(Added);

	UE_LOG(LogQA, Display, TEXT("QA: quest OFFERED (%s, kill %d %s, item %d '%s')"),
		*Added.QuestId.ToString(), Added.TargetCount, *Added.KillTargetTag.ToString(),
		Added.RequiredItemCount, *Added.RequiredItemName);

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

	UE_LOG(LogQA, Display, TEXT("QA: quest ACCEPTED (%s, kill %d %s, item %d '%s')"),
		*Q->QuestId.ToString(), Q->TargetCount, *Q->KillTargetTag.ToString(),
		Q->RequiredItemCount, *Q->RequiredItemName);

	// На случай, если предметы уже в рюкзаке к моменту принятия (или kill-цели нет) — сразу пересчёт.
	const bool bChanged = RecomputeState(*Q);
	OnQuestChanged.Broadcast(*Q);
	(void)bChanged;
	return true;
}

void UQuestComponent::NotifyKill(FName TargetTag)
{
	for (FQuest& Q : Quests)
	{
		if (Q.State != EQuestState::Active)
		{
			continue;
		}
		// Учитываем только квесты с kill-целью (TargetCount>0).
		if (Q.TargetCount <= 0)
		{
			continue;
		}
		// Пустой KillTargetTag = любая цель; иначе должен совпадать тег.
		if (!Q.KillTargetTag.IsNone() && Q.KillTargetTag != TargetTag)
		{
			continue;
		}

		Q.Progress = FMath::Min(Q.Progress + 1, Q.TargetCount);
		UE_LOG(LogQA, Display, TEXT("QA: quest %s kill progress %d/%d"),
			*Q.QuestId.ToString(), Q.Progress, Q.TargetCount);

		RecomputeState(Q); // может перевести в Completed (лог внутри)
		OnQuestChanged.Broadcast(Q);
	}
}

void UQuestComponent::SyncInventoryQuests(UInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return;
	}

	for (FQuest& Q : Quests)
	{
		// Только активная фаза и только квесты с item-целью.
		if (Q.RequiredItemCount <= 0)
		{
			continue;
		}
		if (Q.State != EQuestState::Active && Q.State != EQuestState::Completed)
		{
			continue;
		}

		// Считаем предметы рюкзака с нужным именем (регистр учитывается; имена задаём детерминированно).
		int32 Count = 0;
		for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
		{
			if (Item && Item->ItemName.Equals(Q.RequiredItemName, ESearchCase::CaseSensitive))
			{
				++Count;
			}
		}

		const int32 NewProgress = FMath::Min(Count, Q.RequiredItemCount);
		bool bChanged = false;
		if (NewProgress != Q.ItemProgress)
		{
			Q.ItemProgress = NewProgress;
			bChanged = true;
			UE_LOG(LogQA, Display, TEXT("QA: quest %s item progress %d/%d ('%s')"),
				*Q.QuestId.ToString(), Q.ItemProgress, Q.RequiredItemCount, *Q.RequiredItemName);
		}

		const bool bStateChanged = RecomputeState(Q);
		if (bChanged || bStateChanged)
		{
			OnQuestChanged.Broadcast(Q);
		}
	}
}

bool UQuestComponent::TurnInQuest(FName QuestId)
{
	FQuest* Q = FindQuestMutable(QuestId);
	if (!Q || Q->State != EQuestState::Completed)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();

	// Если у квеста есть item-цель — изымаем предметы из рюкзака владельца.
	if (Q->RequiredItemCount > 0)
	{
		UInventoryComponent* Inv = OwnerActor ? OwnerActor->FindComponentByClass<UInventoryComponent>() : nullptr;
		if (!Inv)
		{
			UE_LOG(LogQA, Display, TEXT("QA: turn-in FAILED (%s) - no inventory on owner"), *Q->QuestId.ToString());
			return false;
		}

		// Собираем нужное число предметов с совпадающим именем.
		TArray<AMasterInventoryItem*> ToRemove;
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (Item && Item->ItemName.Equals(Q->RequiredItemName, ESearchCase::CaseSensitive))
			{
				ToRemove.Add(Item);
				if (ToRemove.Num() >= Q->RequiredItemCount)
				{
					break;
				}
			}
		}

		if (ToRemove.Num() < Q->RequiredItemCount)
		{
			UE_LOG(LogQA, Display, TEXT("QA: turn-in FAILED (%s) - need %d '%s', have %d"),
				*Q->QuestId.ToString(), Q->RequiredItemCount, *Q->RequiredItemName, ToRemove.Num());
			return false;
		}

		for (AMasterInventoryItem* Item : ToRemove)
		{
			Inv->RemoveItem(Item);
			if (IsValid(Item))
			{
				Item->Destroy();
			}
		}
		UE_LOG(LogQA, Display, TEXT("QA: turn-in took %d x '%s' from backpack (%s)"),
			Q->RequiredItemCount, *Q->RequiredItemName, *Q->QuestId.ToString());
	}

	Q->State = EQuestState::TurnedIn;

	// Награда деньгами начисляется в UStatsComponent владельца (игрока).
	float Balance = -1.0f;
	if (OwnerActor)
	{
		if (UStatsComponent* Stats = OwnerActor->FindComponentByClass<UStatsComponent>())
		{
			Stats->AddMoney(Q->RewardMoney);
			Balance = Stats->GetMoney();
		}
	}

	UE_LOG(LogQA, Display, TEXT("QA: quest TURNED IN (%s), +%.0f money, balance %.0f"),
		*Q->QuestId.ToString(), Q->RewardMoney, Balance);

	OnQuestChanged.Broadcast(*Q);
	return true;
}
