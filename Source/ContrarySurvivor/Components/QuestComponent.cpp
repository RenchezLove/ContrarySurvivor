// Fill out your copyright notice in the Description page of Project Settings.

#include "QuestComponent.h"
#include "ContrarySurvivor/Components/StatsComponent.h"
#include "ContrarySurvivor/ContrarySurvivor.h" // LogQA
#include "ContrarySurvivor/Analytics/AnalyticsSubsystem.h" // F3: события взятия/сдачи квеста
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

	// F3 (ADR-038): событие «взятие квеста». Хук именно здесь (не на OnQuestChanged): делегат
	// стреляет и на прогрессе, а нам нужен один факт принятия. Без ключей — no-op.
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordQuestAccepted(QuestId);
	}
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

		// Считаем предметы рюкзака с нужным именем (регистр учитывается; имена задаём
		// детерминированно). Build 1.2.1 (ТЗ Г): стак считается ПО ШТУКАМ — «Шкура волка» x3
		// в одном акторе засчитывает 3 (Max(1,...): нестакаемый актор = 1, как раньше).
		int32 Count = 0;
		for (const AMasterInventoryItem* Item : Inventory->GetInventoryItems())
		{
			if (Item && Item->ItemName.Equals(Q.RequiredItemName, ESearchCase::CaseSensitive))
			{
				Count += FMath::Max(1, Item->GetStackCount());
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

		// Собираем предметы с совпадающим именем НА НУЖНОЕ ЧИСЛО ШТУК. Build 1.2.1 (ТЗ Г):
		// предмет может быть стаком — с актора берём до GetStackCount() штук; если стак
		// больше остатка требования, изымаем ЧАСТИЧНО (уменьшаем счётчик, актор живёт).
		TArray<AMasterInventoryItem*> ToRemove;   // акторы, уходящие целиком
		AMasterInventoryItem* PartialFrom = nullptr; // стак, из которого берём часть
		int32 PartialTake = 0;
		int32 Collected = 0;
		for (AMasterInventoryItem* Item : Inv->GetInventoryItems())
		{
			if (Collected >= Q->RequiredItemCount)
			{
				break;
			}
			if (!Item || !Item->ItemName.Equals(Q->RequiredItemName, ESearchCase::CaseSensitive))
			{
				continue;
			}
			const int32 Available = FMath::Max(1, Item->GetStackCount());
			const int32 Need = Q->RequiredItemCount - Collected;
			if (Available <= Need)
			{
				ToRemove.Add(Item);
				Collected += Available;
			}
			else
			{
				PartialFrom = Item;
				PartialTake = Need;
				Collected += Need;
			}
		}

		if (Collected < Q->RequiredItemCount)
		{
			UE_LOG(LogQA, Display, TEXT("QA: turn-in FAILED (%s) - need %d '%s', have %d"),
				*Q->QuestId.ToString(), Q->RequiredItemCount, *Q->RequiredItemName, Collected);
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
		if (PartialFrom && PartialTake > 0)
		{
			PartialFrom->StackCount = FMath::Max(0, PartialFrom->StackCount - PartialTake);
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

	// F3 (ADR-038): событие «сдача квеста» (с id квеста). Без ключей — no-op.
	if (UAnalyticsSubsystem* Analytics = UAnalyticsSubsystem::Get(this))
	{
		Analytics->RecordQuestTurnedIn(QuestId);
	}
	return true;
}

void UQuestComponent::RestoreQuests(const TArray<FQuest>& SavedQuests)
{
	// Б3 («Продолжить»): журнал заменяется целиком сохранённым снимком — квесты, ещё не
	// предложенные в ЭТОЙ сессии (игрок не успел снова подойти к старосте), уже показывают
	// верный заголовок/прогресс/состояние, а не пустую заглушку до следующего OfferQuest.
	Quests = SavedQuests;

	for (const FQuest& Q : Quests)
	{
		OnQuestChanged.Broadcast(Q);
	}

	UE_LOG(LogQA, Display, TEXT("QA: quest journal restored from save (%d quest(s))"), Quests.Num());
}
